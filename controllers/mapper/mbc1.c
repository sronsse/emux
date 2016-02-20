#include <stdlib.h>
#include <bitops.h>
#include <controller.h>
#include <file.h>
#include <memory.h>
#include <util.h>
#include "gb_mapper.h"

#define RAM_ENABLE_START	0x0000
#define RAM_ENABLE_END		0x1FFF
#define ROM_NUM_LOW_START	0x2000
#define ROM_NUM_LOW_END		0x3FFF
#define ROM_NUM_HIGH_START	0x4000
#define ROM_NUM_HIGH_END	0x5FFF
#define MODE_SELECT_START	0x6000
#define MODE_SELECT_END		0x7FFF

#define ROM_SELECT_MODE	0
#define RAM_SELECT_MODE	1

struct mbc1 {
	int rom_size;
	int ram_size;
	uint8_t *rom;
	uint8_t *ram;
	uint8_t rom_num_low;
	uint8_t rom_num_high;
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

static bool mbc1_init(struct controller_instance *instance);
static void mbc1_reset(struct controller_instance *instance);
static void mbc1_deinit(struct controller_instance *instance);
static uint8_t rom1_readb(struct mbc1 *mbc1, address_t address);
static uint8_t extram_readb(struct mbc1 *mbc1, address_t address);
static void extram_writeb(struct mbc1 *mbc1, uint8_t b, address_t address);
static void ram_en_writeb(struct mbc1 *mbc1, uint8_t b, address_t address);
static void rom_low_writeb(struct mbc1 *mbc1, uint8_t b, address_t address);
static void rom_high_writeb(struct mbc1 *mbc1, uint8_t b, address_t address);
static void mode_sel_writeb(struct mbc1 *mbc1, uint8_t b, address_t address);

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

static struct mops rom_high_mops = {
	.writeb = (writeb_t)rom_high_writeb
};

static struct mops mode_sel_mops = {
	.writeb = (writeb_t)mode_sel_writeb
};

uint8_t rom1_readb(struct mbc1 *mbc1, address_t address)
{
	uint8_t rom_num;
	int offset;

	/* Set ROM bank number (bits 5-6 depend on mode selection) */
	rom_num = mbc1->rom_num_low;
	if (mbc1->mode_sel == ROM_SELECT_MODE)
		bitops_setb(&rom_num, 5, 2, mbc1->rom_num_high);

	/* Adapt address (skipping ROM0) and return ROM contents */
	offset = address + (rom_num - 1) * ROM_BANK_SIZE;
	return mbc1->rom[offset];
}

uint8_t extram_readb(struct mbc1 *mbc1, address_t address)
{
	uint8_t ram_num;
	int offset;

	/* Return already if RAM is disabled */
	if (!mbc1->ram_enabled)
		return 0;

	/* Set RAM bank number depending on mode selection */
	ram_num = (mbc1->mode_sel == RAM_SELECT_MODE) ? mbc1->rom_num_high : 0;

	/* Adapt address and return RAM contents */
	offset = address + ram_num * RAM_BANK_SIZE;
	return mbc1->ram[offset];
}

void extram_writeb(struct mbc1 *mbc1, uint8_t b, address_t address)
{
	uint8_t ram_num;
	int offset;

	/* Return already if RAM is disabled */
	if (!mbc1->ram_enabled)
		return;

	/* Set RAM bank number depending on mode selection */
	ram_num = (mbc1->mode_sel == RAM_SELECT_MODE) ? mbc1->rom_num_high : 0;

	/* Adapt address and write RAM contents */
	offset = address + ram_num * RAM_BANK_SIZE;
	mbc1->ram[offset] = b;
}

void ram_en_writeb(struct mbc1 *mbc1, uint8_t b, address_t UNUSED(address))
{
	uint8_t ram_enable;

	/* Any value with 0xOA in the lower 4 bits enables RAM */
	ram_enable = bitops_getb(&b, 0, 4);
	mbc1->ram_enabled = (ram_enable == 0x0A);
}

void rom_low_writeb(struct mbc1 *mbc1, uint8_t b, address_t UNUSED(address))
{
	/* Data represents lower 5 bits of ROM bank number */
	mbc1->rom_num_low = bitops_getb(&b, 0, 5);

	/* MBC translates bank number 0 to 1 */
	if (mbc1->rom_num_low == 0)
		mbc1->rom_num_low = 1;
}

void rom_high_writeb(struct mbc1 *mbc1, uint8_t b, address_t UNUSED(address))
{
	/* Set RAM or upper ROM bank number (register size is 2 bits) */
	mbc1->rom_num_high = bitops_getb(&b, 0, 2);
}

void mode_sel_writeb(struct mbc1 *mbc1, uint8_t b, address_t UNUSED(address))
{
	/* Set mode selection (register size is 1 bit) */
	mbc1->mode_sel = bitops_getb(&b, 0, 1);
}

bool mbc1_init(struct controller_instance *instance)
{
	struct mbc1 *mbc1;
	struct cart_header *cart_header;
	struct resource *area;
	char *cart_path;

	/* Allocate MBC1 structure */
	instance->priv_data = calloc(1, sizeof(struct mbc1));
	mbc1 = instance->priv_data;

	/* Retrieve cart path (via machine data) */
	cart_path = instance->mach_data;

	/* Map cart header */
	cart_header = file_map(PATH_DATA,
		cart_path,
		CART_HEADER_START,
		sizeof(struct cart_header));

	/* Get ROM size to map (minus already mappped ROM0) */
	mbc1->rom_size = gb_mapper_get_rom_size(cart_header) - ROM_BANK_SIZE;

	/* Map ROM contents (skip ROM0) */
	mbc1->rom = file_map(PATH_DATA,
		cart_path,
		ROM_BANK_SIZE,
		mbc1->rom_size);

	/* Add ROM area */
	area = resource_get("rom1",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	mbc1->rom1_region.area = area;
	mbc1->rom1_region.mops = &rom1_mops;
	mbc1->rom1_region.data = mbc1;
	memory_region_add(&mbc1->rom1_region);

	/* Get RAM size to map */
	mbc1->ram_size = gb_mapper_get_ram_size(cart_header);

	/* Map RAM if needed */
	if (mbc1->ram_size != 0) {
		/* Allocate RAM */
		mbc1->ram = calloc(mbc1->ram_size, sizeof(uint8_t));

		/* Add RAM region */
		area = resource_get("extram",
			RESOURCE_MEM,
			instance->resources,
			instance->num_resources);
		mbc1->extram_region.area = area;
		mbc1->extram_region.mops = &extram_mops;
		mbc1->extram_region.data = mbc1;
		memory_region_add(&mbc1->extram_region);
	}

	/* Add RAM enable region */
	mbc1->ram_en_area.type = RESOURCE_MEM;
	mbc1->ram_en_area.data.mem.bus_id = instance->bus_id;
	mbc1->ram_en_area.data.mem.start = RAM_ENABLE_START;
	mbc1->ram_en_area.data.mem.end = RAM_ENABLE_END;
	mbc1->ram_en_area.num_children = 0;
	mbc1->ram_en_area.children = NULL;
	mbc1->ram_en_region.area = &mbc1->ram_en_area;
	mbc1->ram_en_region.mops = &ram_en_mops;
	mbc1->ram_en_region.data = mbc1;
	memory_region_add(&mbc1->ram_en_region);

	/* Add ROM bank selection region */
	mbc1->rom_low_area.type = RESOURCE_MEM;
	mbc1->rom_low_area.data.mem.bus_id = instance->bus_id;
	mbc1->rom_low_area.data.mem.start = ROM_NUM_LOW_START;
	mbc1->rom_low_area.data.mem.end = ROM_NUM_LOW_END;
	mbc1->rom_low_area.num_children = 0;
	mbc1->rom_low_area.children = NULL;
	mbc1->rom_low_region.area = &mbc1->rom_low_area;
	mbc1->rom_low_region.mops = &rom_low_mops;
	mbc1->rom_low_region.data = mbc1;
	memory_region_add(&mbc1->rom_low_region);

	/* Add RAM/ROM bank selection region */
	mbc1->rom_high_area.type = RESOURCE_MEM;
	mbc1->rom_high_area.data.mem.bus_id = instance->bus_id;
	mbc1->rom_high_area.data.mem.start = ROM_NUM_HIGH_START;
	mbc1->rom_high_area.data.mem.end = ROM_NUM_HIGH_END;
	mbc1->rom_high_area.num_children = 0;
	mbc1->rom_high_area.children = NULL;
	mbc1->rom_high_region.area = &mbc1->rom_high_area;
	mbc1->rom_high_region.mops = &rom_high_mops;
	mbc1->rom_high_region.data = mbc1;
	memory_region_add(&mbc1->rom_high_region);

	/* Add mode selection region */
	mbc1->mode_sel_area.type = RESOURCE_MEM;
	mbc1->mode_sel_area.data.mem.bus_id = instance->bus_id;
	mbc1->mode_sel_area.data.mem.start = MODE_SELECT_START;
	mbc1->mode_sel_area.data.mem.end = MODE_SELECT_END;
	mbc1->mode_sel_area.num_children = 0;
	mbc1->mode_sel_area.children = NULL;
	mbc1->mode_sel_region.area = &mbc1->mode_sel_area;
	mbc1->mode_sel_region.mops = &mode_sel_mops;
	mbc1->mode_sel_region.data = mbc1;
	memory_region_add(&mbc1->mode_sel_region);

	/* Unmap cart header */
	file_unmap(cart_header, sizeof(struct cart_header));

	/* Save private data */
	instance->priv_data = mbc1;

	return true;
}

void mbc1_reset(struct controller_instance *instance)
{
	struct mbc1 *mbc1 = instance->priv_data;

	/* Initialize private data */
	mbc1->rom_num_low = 1;
	mbc1->rom_num_high = 0;
	mbc1->ram_enabled = false;
	mbc1->mode_sel = ROM_SELECT_MODE;
}

void mbc1_deinit(struct controller_instance *instance)
{
	struct mbc1 *mbc1 = instance->priv_data;

	/* Free RAM contents */
	if (mbc1->ram_size != 0)
		free(mbc1->ram);

	/* Unmap ROM contents */
	file_unmap(mbc1->rom, mbc1->rom_size);

	/* Free MBC1 structure */
	free(mbc1);
}

CONTROLLER_START(mbc1)
	.init = mbc1_init,
	.reset = mbc1_reset,
	.deinit = mbc1_deinit
CONTROLLER_END

