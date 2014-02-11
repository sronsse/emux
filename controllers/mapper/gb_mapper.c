#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <controller.h>
#include <file.h>
#include <log.h>
#include <memory.h>
#include <util.h>
#include <controllers/mapper/gb_mapper.h>

#define CART_HEADER_START 0x0100

struct gb_mapper {
	struct gb_mapper_mach_data *mach_data;
	struct resource *bootrom_area;
	struct resource *rom0_area;
	uint8_t *bootrom;
	uint16_t bootrom_size;
	uint8_t *rom0;
	uint16_t rom0_size;
	bool bootrom_locked;
	struct controller_instance *mbc_instance;
};

static bool gb_mapper_init(struct controller_instance *instance);
static void gb_mapper_deinit(struct controller_instance *instance);
static bool map_bootrom(struct gb_mapper *gb_mapper);
static bool map_rom0(struct gb_mapper *gb_mapper);
static void lock_writeb(region_data_t *data, uint8_t b, address_t address);

static char *mbcs[] = {
	"rom"	/* ROM ONLY */
};

static struct mops lock_mops = {
	.writeb = lock_writeb
};

void lock_writeb(region_data_t *data, uint8_t b, address_t UNUSED(address))
{
	struct gb_mapper *gb_mapper = data;

	/* Check if locking is actually requested and needed */
	if ((b == 0) || gb_mapper->bootrom_locked)
		return;

	/* Update state */
	gb_mapper->bootrom_locked = true;

	/* Unmap boot ROM and remove its region */
	file_unmap(gb_mapper->bootrom, gb_mapper->bootrom_size);
	memory_region_remove(gb_mapper->bootrom_area);

	/* Unmap ROM0 */
	file_unmap(gb_mapper->rom0, gb_mapper->rom0_size);
	memory_region_remove(gb_mapper->rom0_area);

	/* Remap ROM0 after changing its start address */
	gb_mapper->rom0_area->data.mem.start -= gb_mapper->bootrom_size;
	map_rom0(gb_mapper);
}

bool map_bootrom(struct gb_mapper *gb_mapper)
{
	/* Compute boot ROM size from resource */
	gb_mapper->bootrom_size = gb_mapper->bootrom_area->data.mem.end -
		gb_mapper->bootrom_area->data.mem.start + 1;

	/* Map boot ROM */
	gb_mapper->bootrom = file_map(PATH_SYSTEM,
		gb_mapper->mach_data->bootrom_path, 0, gb_mapper->bootrom_size);

	/* Check if mapping was successful */
	if (!gb_mapper->bootrom)
		return false;

	/* Add boot ROM memory region */
	memory_region_add(gb_mapper->bootrom_area, &rom_mops,
		gb_mapper->bootrom);

	/* Set boot ROM state */
	gb_mapper->bootrom_locked = false;

	return true;
}

bool map_rom0(struct gb_mapper *gb_mapper)
{
	int offset;

	/* Compute mapped size */
	gb_mapper->rom0_size = gb_mapper->rom0_area->data.mem.end -
		gb_mapper->rom0_area->data.mem.start + 1;

	/* Set offset based on boot ROM state */
	offset = gb_mapper->bootrom_locked ? 0 : gb_mapper->bootrom_size;

	/* Map ROM0 after boot ROM */
	gb_mapper->rom0 = file_map(PATH_DATA, gb_mapper->mach_data->cart_path,
		offset, gb_mapper->rom0_size);
	if (!gb_mapper->rom0) {
		LOG_E("Could not map cart from \"%s\"!\n",
			gb_mapper->mach_data->cart_path);
		return false;
	}

	/* Add ROM0 memory region */
	memory_region_add(gb_mapper->rom0_area, &rom_mops, gb_mapper->rom0);
	return true;
}

bool gb_mapper_init(struct controller_instance *instance)
{
	struct gb_mapper *gb_mapper;
	struct controller_instance *mbc_instance;
	struct cart_header *cart_header;
	uint8_t number;
	struct resource *lock_area;

	/* Allocate NROM structure */
	instance->priv_data = malloc(sizeof(struct gb_mapper));
	gb_mapper = instance->priv_data;

	/* Save machine data */
	gb_mapper->mach_data = instance->mach_data;

	/* Map cart header */
	cart_header = file_map(PATH_DATA, gb_mapper->mach_data->cart_path,
		CART_HEADER_START, sizeof(struct cart_header));
	if (!cart_header) {
		LOG_E("Could not map header from \"%s\"!\n",
			gb_mapper->mach_data->cart_path);
		free(gb_mapper);
		return false;
	}

	/* Print header info */
	LOG_I("Title: %.*s\n", TITLE_SIZE, cart_header->title);
	LOG_I("Manufacturer code: %.*s\n", MANUFACTURER_CODE_SIZE,
		cart_header->manufacturer_code);
	LOG_I("CGB flag: %u\n", cart_header->cgb_flag);
	LOG_I("New licensee code: %.*s\n", NEW_LICENSEE_CODE_SIZE,
		cart_header->new_licensee_code);
	LOG_I("SGB flag: %u\n", cart_header->sgb_flag);
	LOG_I("Cartridge type: %02x\n", cart_header->cartridge_type);
	LOG_I("ROM size: %02x\n", cart_header->rom_size);
	LOG_I("RAM size: %02x\n", cart_header->ram_size);
	LOG_I("Destination code: %02x\n", cart_header->dest_code);
	LOG_I("Old licensee code: %02x\n", cart_header->old_licensee_code);
	LOG_I("ROM version: %02x\n", cart_header->rom_version);
	LOG_I("Header checksum: %02x\n", cart_header->header_checksum);
	LOG_I("Global checksum: %04x\n", cart_header->global_checksum);

	/* Get cart type number */
	number = cart_header->cartridge_type;

	/* Unmap cart header */
	file_unmap(cart_header, sizeof(struct cart_header));

	/* Check if cart type is supported */
	if ((number >= ARRAY_SIZE(mbcs)) || !mbcs[number]) {
		LOG_E("Cart type %u is not supported!\n", number);
		free(gb_mapper);
		return false;
	}

	/* Get bootrom and ROM0 areas */
	gb_mapper->bootrom_area = resource_get("bootrom",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	gb_mapper->rom0_area = resource_get("rom0",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);

	/* Map boot ROM */
	if (!map_bootrom(gb_mapper)) {
		free(gb_mapper);
		LOG_E("Could not map boot ROM!\n");
		return false;
	}

	/* Map ROM0 after changing its start address */
	gb_mapper->rom0_area->data.mem.start += gb_mapper->bootrom_size;
	if (!map_rom0(gb_mapper)) {
		file_unmap(gb_mapper->bootrom, gb_mapper->bootrom_size);
		free(gb_mapper);
		LOG_E("Could not map ROM0!\n");
		return false;
	}

	/* Add lock region */
	lock_area = resource_get("lock",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	memory_region_add(lock_area, &lock_mops, gb_mapper);

	/* Cart type is supported, so add actual controller */
	LOG_I("Cart type %u (%s) detected.\n", number, mbcs[number]);
	mbc_instance = malloc(sizeof(struct controller_instance));
	mbc_instance->controller_name = mbcs[number];
	mbc_instance->num_resources = instance->num_resources;
	mbc_instance->resources = instance->resources;
	mbc_instance->mach_data = instance->mach_data;
	gb_mapper->mbc_instance = mbc_instance;
	controller_add(mbc_instance);

	return true;
}

void gb_mapper_deinit(struct controller_instance *instance)
{
	struct gb_mapper *gb_mapper = instance->priv_data;

	/* Unmap boot ROM if needed */
	if (!gb_mapper->bootrom_locked)
		file_unmap(gb_mapper->bootrom, gb_mapper->bootrom_size);

	/* Unmap ROM0 */
	file_unmap(gb_mapper->rom0, gb_mapper->rom0_size);

	/* Free allocated structures */
	free(gb_mapper->mbc_instance);
	free(gb_mapper);
}

CONTROLLER_START(gb_mapper)
	.init = gb_mapper_init,
	.deinit = gb_mapper_deinit
CONTROLLER_END

