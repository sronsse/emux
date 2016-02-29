#include <stdlib.h>
#include <bitops.h>
#include <controller.h>
#include <file.h>
#include <memory.h>
#include <util.h>
#include <controllers/mapper/gb_mapper.h>

#define RAM_ENABLE_START	0x0000
#define RAM_ENABLE_END		0x1FFF
#define ROM_NUM_START		0x2000
#define ROM_NUM_END			0x3FFF
#define MODE_SELECT_START	0x6000
#define MODE_SELECT_END		0x7FFF


struct mbc2 {
	int rom_size;
	int ram_size;
	uint8_t *rom;
	uint8_t *ram;
	uint8_t rom_num;
	bool ram_enabled;
	int mode_sel;
	struct resource ram_en_area;
	struct resource rom_low_area;
	struct resource rom_high_area;
	struct resource mode_sel_area;
	struct region rom1_region;
	struct region extram_region;
	struct region ram_en_region;
	struct region rom_low_region;
	struct region rom_high_region;
	struct region mode_sel_region;
};

static bool mbc2_init(struct controller_instance *instance);
static void mbc2_reset(struct controller_instance *instance);
static void mbc2_deinit(struct controller_instance *instance);
static uint8_t rom1_readb(struct mbc2 *mbc2, address_t address);
static uint8_t extram_readb(struct mbc2 *mbc2, address_t address);
static void extram_writeb(struct mbc2 *mbc2, uint8_t b, address_t address);
static void ram_en_writeb(struct mbc2 *mbc2, uint8_t b, address_t address);
static void rom_low_writeb(struct mbc2 *mbc2, uint8_t b, address_t address);

static struct mops rom1_mops = {
	.readb = (readb_t)rom1_readb
};

static struct mops extram_mops = {
	.readb = (readb_t)extram_readb,
	.writeb = (writeb_t)extram_writeb
};

static struct mops ram_en_mops = {
	.writeb = (writeb_t)ram_en_writeb
};

static struct mops rom_low_mops = {
	.writeb = (writeb_t)rom_low_writeb
};

uint8_t rom1_readb(struct mbc2 *mbc2, address_t address)
{
	uint8_t rom_num;
	int offset;

	/* Set ROM bank number (bits 5-6 depend on mode selection) */
	rom_num = mbc2->rom_num;

	/* Adapt address (skipping ROM0) and return ROM contents */
	offset = address + (rom_num - 1) * ROM_BANK_SIZE;
	return mbc2->rom[offset];
}

uint8_t extram_readb(struct mbc2 *mbc2, address_t address)
{
	int offset;

	/* Return already if RAM is disabled */
	if (!mbc2->ram_enabled)
		return 0;

	/* Adapt address and return RAM contents */
	offset = address;
	return mbc2->ram[offset];
}

void extram_writeb(struct mbc2 *mbc2, uint8_t b, address_t address)
{
	int offset;

	/* Return already if RAM is disabled */
	if (!mbc2->ram_enabled)
		return;

	/* Adapt address and write RAM contents */
	offset = address;
	mbc2->ram[offset] = b;
}

void ram_en_writeb(struct mbc2 *mbc2, uint8_t b, address_t UNUSED(address))
{
	uint8_t ram_enable;

	/* Any value with 0xOA in the lower 4 bits enables RAM */
	ram_enable = bitops_getb(&b, 0, 4);
	mbc2->ram_enabled = (ram_enable == 0x0A);
}

void rom_low_writeb(struct mbc2 *mbc2, uint8_t b, address_t UNUSED(address))
{
	/* Data represents lower 5 bits of ROM bank number */
	mbc2->rom_num = bitops_getb(&b, 0, 4);

	/* MBC translates bank number 0 to 1 */
	if (mbc2->rom_num == 0)
		mbc2->rom_num = 1;
}

bool mbc2_init(struct controller_instance *instance)
{
	struct gb_mapper_mach_data *mach_data = instance->mach_data;
	struct mbc2 *mbc2;
	struct cart_header *cart_header;
	struct resource *area;

	/* Allocate mbc2 structure */
	instance->priv_data = calloc(1, sizeof(struct mbc2));
	mbc2 = instance->priv_data;

	/* Map cart header */
	cart_header = file_map(PATH_DATA,
		mach_data->cart_path,
		CART_HEADER_START,
		sizeof(struct cart_header));

	/* Get ROM size to map (minus already mappped ROM0) */
	mbc2->rom_size = gb_mapper_get_rom_size(cart_header) - ROM_BANK_SIZE;

	/* Map ROM contents (skip ROM0) */
	mbc2->rom = file_map(PATH_DATA,
		mach_data->cart_path,
		ROM_BANK_SIZE,
		mbc2->rom_size);

	/* Add ROM area */
	area = resource_get("rom1",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	mbc2->rom1_region.area = area;
	mbc2->rom1_region.mops = &rom1_mops;
	mbc2->rom1_region.data = mbc2;
	memory_region_add(&mbc2->rom1_region);

	/* Get RAM size to map */
	mbc2->ram_size = gb_mapper_get_ram_size(cart_header);

	/* Map RAM if needed */
	if (mbc2->ram_size != 0) {
		/* Allocate RAM */
		mbc2->ram = calloc(mbc2->ram_size, sizeof(uint8_t));

		/* Add RAM region */
		area = resource_get("extram",
			RESOURCE_MEM,
			instance->resources,
			instance->num_resources);
		mbc2->extram_region.area = area;
		mbc2->extram_region.mops = &extram_mops;
		mbc2->extram_region.data = mbc2;
		memory_region_add(&mbc2->extram_region);
	}

	/* Add RAM enable region */
	mbc2->ram_en_area.type = RESOURCE_MEM;
	mbc2->ram_en_area.data.mem.bus_id = instance->bus_id;
	mbc2->ram_en_area.data.mem.start = RAM_ENABLE_START;
	mbc2->ram_en_area.data.mem.end = RAM_ENABLE_END;
	mbc2->ram_en_area.num_children = 0;
	mbc2->ram_en_area.children = NULL;
	mbc2->ram_en_region.area = &mbc2->ram_en_area;
	mbc2->ram_en_region.mops = &ram_en_mops;
	mbc2->ram_en_region.data = mbc2;
	memory_region_add(&mbc2->ram_en_region);

	/* Add ROM bank selection region */
	mbc2->rom_low_area.type = RESOURCE_MEM;
	mbc2->rom_low_area.data.mem.bus_id = instance->bus_id;
	mbc2->rom_low_area.data.mem.start = ROM_NUM_START;
	mbc2->rom_low_area.data.mem.end = ROM_NUM_END;
	mbc2->rom_low_area.num_children = 0;
	mbc2->rom_low_area.children = NULL;
	mbc2->rom_low_region.area = &mbc2->rom_low_area;
	mbc2->rom_low_region.mops = &rom_low_mops;
	mbc2->rom_low_region.data = mbc2;
	memory_region_add(&mbc2->rom_low_region);

	/* Unmap cart header */
	file_unmap(cart_header, sizeof(struct cart_header));

	/* Save private data */
	instance->priv_data = mbc2;

	return true;
}

void mbc2_reset(struct controller_instance *instance)
{
	struct mbc2 *mbc2 = instance->priv_data;

	/* Initialize private data */
	mbc2->rom_num = 1;
	mbc2->ram_enabled = false;
}

void mbc2_deinit(struct controller_instance *instance)
{
	struct mbc2 *mbc2 = instance->priv_data;

	/* Free RAM contents */
	if (mbc2->ram_size != 0)
		free(mbc2->ram);

	/* Unmap ROM contents */
	file_unmap(mbc2->rom, mbc2->rom_size);

	/* Free mbc2 structure */
	free(mbc2);
}

CONTROLLER_START(mbc2)
	.init = mbc2_init,
	.reset = mbc2_reset,
	.deinit = mbc2_deinit
CONTROLLER_END

