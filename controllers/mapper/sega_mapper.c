#include <stdlib.h>
#include <controller.h>
#include <file.h>
#include <log.h>
#include <controllers/mapper/sms_mapper.h>

#define NUM_BANKS	3
#define PAGE_OFFSET	0x0400
#define ROM_START	0x0000
#define ROM_END		0xBFFF
#define ROM_SEL_START	0xFFFD
#define ROM_SEL_END	0xFFFF
#define BANK_SIZE	0x4000

struct sega_mapper {
	uint8_t *rom;
	int rom_size;
	struct resource rom_area;
	struct resource rom_sel_area;
	struct region rom_sel_region;
	uint8_t rom_banks[NUM_BANKS];
};

static bool sega_mapper_init(struct controller_instance *instance);
static void sega_mapper_reset(struct controller_instance *instance);
static void sega_mapper_deinit(struct controller_instance *instance);
static uint8_t sega_rom_readb(struct sega_mapper *mapper, address_t a);
static void rom_sel_writeb(struct sega_mapper *mapper,  uint8_t b, address_t a);

static struct mops sega_rom_mops = {
	.readb = (readb_t)sega_rom_readb
};

static struct mops rom_sel_mops = {
	.writeb = (writeb_t)rom_sel_writeb
};

uint8_t sega_rom_readb(struct sega_mapper *mapper, address_t address)
{
	uint8_t slot;
	int offset;

	/* Get slot number based on address */
	slot = address / BANK_SIZE;

	/* Adapt address (first page cannot be swapped out) */
	offset = address;
	if (address >= PAGE_OFFSET)
		offset += (mapper->rom_banks[slot] - slot) * BANK_SIZE;

	return mapper->rom[offset];
}

void rom_sel_writeb(struct sega_mapper *mapper, uint8_t b, address_t address)
{
	uint8_t slot;

	/* Mask most significant bits based on ROM size */
	slot = b & ((mapper->rom_size / BANK_SIZE) - 1);

	/* Update ROM bank number */
	mapper->rom_banks[address] = slot;
}

bool sega_mapper_init(struct controller_instance *instance)
{
	struct sega_mapper *sega_mapper;
	struct cart_mapper_mach_data *mach_data;
	struct region *region;
	file_handle_t file;

	/* Allocate SEGA mapper structure */
	instance->priv_data = malloc(sizeof(struct sega_mapper));
	sega_mapper = instance->priv_data;

	/* Get cart region */
	mach_data = instance->mach_data;
	region = mach_data->cart_region;

	/* Open cart file */
	file = file_open(PATH_DATA, mach_data->cart_path, "rb");
	if (!file) {
		LOG_E("Could not open cart!\n");
		free(sega_mapper);
		return false;
	}

	/* Get cart size */
	sega_mapper->rom_size = file_get_size(file);

	/* Close cart file */
	file_close(file);

	/* Map ROM contents */
	sega_mapper->rom = file_map(PATH_DATA,
		mach_data->cart_path,
		0,
		sega_mapper->rom_size);

	/* Fill cart region */
	sega_mapper->rom_area.type = RESOURCE_MEM;
	sega_mapper->rom_area.data.mem.bus_id = instance->bus_id;
	sega_mapper->rom_area.data.mem.start = ROM_START;
	sega_mapper->rom_area.data.mem.end = ROM_END;
	sega_mapper->rom_area.num_children = 0;
	sega_mapper->rom_area.children = NULL;
	region->area = &sega_mapper->rom_area;
	region->mops = &sega_rom_mops;
	region->data = sega_mapper;

	/* Add ROM select region */
	sega_mapper->rom_sel_area.type = RESOURCE_MEM;
	sega_mapper->rom_sel_area.data.mem.bus_id = instance->bus_id;
	sega_mapper->rom_sel_area.data.mem.start = ROM_SEL_START;
	sega_mapper->rom_sel_area.data.mem.end = ROM_SEL_END;
	sega_mapper->rom_sel_area.num_children = 0;
	sega_mapper->rom_sel_area.children = NULL;
	sega_mapper->rom_sel_region.area = &sega_mapper->rom_sel_area;
	sega_mapper->rom_sel_region.mops = &rom_sel_mops;
	sega_mapper->rom_sel_region.data = sega_mapper;
	memory_region_add(&sega_mapper->rom_sel_region);

	return true;
}

void sega_mapper_reset(struct controller_instance *instance)
{
	struct sega_mapper *sega_mapper = instance->priv_data;
	int i;

	/* Initialize ROM bank numbers */
	for (i = 0; i < NUM_BANKS; i++)
		sega_mapper->rom_banks[i] = i;
}

void sega_mapper_deinit(struct controller_instance *instance)
{
	struct sega_mapper *sega_mapper = instance->priv_data;

	/* Unmap ROM and free private data */
	file_unmap(sega_mapper->rom, sega_mapper->rom_size);
	free(sega_mapper);
}

CONTROLLER_START(sega_mapper)
	.init = sega_mapper_init,
	.reset = sega_mapper_reset,
	.deinit = sega_mapper_deinit
CONTROLLER_END

