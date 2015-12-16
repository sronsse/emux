#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <controller.h>
#include <file.h>
#include <log.h>
#include <memory.h>
#include <util.h>
#include <controllers/mapper/gb_mapper.h>

struct gb_mapper {
	uint8_t *rom0;
	uint8_t *bootrom;
	struct controller_instance *mbc_instance;
	struct region bootrom_region;
	struct region rom0_region;
	struct region lock_region;
	bool bootrom_locked;
};

static bool gb_mapper_init(struct controller_instance *instance);
static void gb_mapper_reset(struct controller_instance *instance);
static void gb_mapper_deinit(struct controller_instance *instance);
static void lock_writeb(struct gb_mapper *gb_mapper, uint8_t b, address_t addr);
static void print_header(struct cart_header *h);
static bool get_mapper_number(char *path, uint8_t *number);
static void add_mapper(struct controller_instance *instance, uint8_t number);

static char *mbcs[] = {
	"rom",	/* ROM ONLY */
	"mbc1",	/* MBC1 */
	"mbc1",	/* MBC1 + RAM */
	"mbc1",	/* MBC1 + RAM + BATTERY */
	"rom",	/* UNKNOWN */
	"mbc2",	/* MBC2 */
	"mbc2", /* MBC2 + BATTERY */
};

static struct mops lock_mops = {
	.writeb = (writeb_t)lock_writeb
};

int gb_mapper_get_rom_size(struct cart_header *cart_header)
{
	/* Return number of banks based on ROM size entry of cart header */
	switch (cart_header->rom_size) {
	case 0x00:
		return KB(32);
	case 0x01:
		return KB(64);
	case 0x02:
		return KB(128);
	case 0x03:
		return KB(256);
	case 0x04:
		return KB(512);
	case 0x05:
		return MB(1);
	case 0x06:
		return MB(2);
	case 0x07:
		return MB(4);
	case 0x52:
		return KB(1152);
	case 0x53:
		return KB(1280);
	case 0x54:
		return KB(1536);
	default:
		return 0;
	}
}

int gb_mapper_get_ram_size(struct cart_header *cart_header)
{
	/* Return number of banks based on RAM size entry of cart header */
	switch (cart_header->ram_size) {
	case 0x00:
		return 0;
	case 0x01:
		return KB(2);
	case 0x02:
		return KB(8);
	case 0x03:
		return KB(32);
	default:
		return 0;
	}
}

void lock_writeb(struct gb_mapper *gb_mapper, uint8_t b, address_t UNUSED(addr))
{
	/* Check if locking is actually requested */
	if (b == 0)
		return;

	/* Remove boot ROM region and update locked state */
	memory_region_remove(&gb_mapper->bootrom_region);
	gb_mapper->bootrom_locked = true;
}

void print_header(struct cart_header *h)
{
	LOG_I("Title: %.*s\n", TITLE_SIZE, h->title);
	LOG_I("Manufacturer code: %.*s\n", MANUFACTURER_SIZE, h->manufacturer);
	LOG_I("CGB flag: %u\n", h->cgb_flag);
	LOG_I("New licensee code: %.*s\n", NEW_LICENSEE_SIZE, h->new_licensee);
	LOG_I("SGB flag: %u\n", h->sgb_flag);
	LOG_I("Cartridge type: %02x\n", h->cartridge_type);
	LOG_I("ROM size: %02x\n", h->rom_size);
	LOG_I("RAM size: %02x\n", h->ram_size);
	LOG_I("Destination code: %02x\n", h->dest_code);
	LOG_I("Old licensee code: %02x\n", h->old_licensee_code);
	LOG_I("ROM version: %02x\n", h->rom_version);
	LOG_I("Header checksum: %02x\n", h->header_checksum);
	LOG_I("Global checksum: %04x\n", h->global_checksum);
}

bool get_mapper_number(char *path, uint8_t *number)
{
	struct cart_header *cart_header;

	/* Map cart header */
	cart_header = file_map(PATH_DATA,
		path,
		CART_HEADER_START,
		sizeof(struct cart_header));

	/* Return already if cart header could not be mapped */
	if (!cart_header)
		return false;

	/* Print header info */
	print_header(cart_header);

	/* Get cart type number */
	*number = cart_header->cartridge_type;

	/* Unmap cart header and return success */
	file_unmap(cart_header, sizeof(struct cart_header));
	return true;
}

void add_mapper(struct controller_instance *instance, uint8_t number)
{
	struct gb_mapper *gb_mapper = instance->priv_data;
	struct controller_instance *mbc_instance;

	mbc_instance = calloc(1, sizeof(struct controller_instance));
	mbc_instance->controller_name = mbcs[number];
	mbc_instance->bus_id = instance->bus_id;
	mbc_instance->num_resources = instance->num_resources;
	mbc_instance->resources = instance->resources;
	mbc_instance->mach_data = instance->mach_data;
	gb_mapper->mbc_instance = mbc_instance;
	controller_add(mbc_instance);
}

bool gb_mapper_init(struct controller_instance *instance)
{
	struct gb_mapper *gb_mapper;
	struct gb_mapper_mach_data *mach_data;
	struct resource *bootrom_area;
	struct resource *rom0_area;
	struct resource *lock_area;
	uint8_t number;

	/* Allocate GB mapper structure */
	instance->priv_data = calloc(1, sizeof(struct gb_mapper));
	gb_mapper = instance->priv_data;

	/* Get mach data from instance */
	mach_data = instance->mach_data;

	/* Get mapper number from cart header */
	if (!get_mapper_number(mach_data->cart_path, &number)) {
		LOG_E("Could not map cart header!\n");
		free(gb_mapper);
		return false;
	}

	/* Check if cart type is supported */
	if ((number >= ARRAY_SIZE(mbcs)) || !mbcs[number]) {
		LOG_E("Cart type %u is not supported!\n", number);
		free(gb_mapper);
		return false;
	}
	LOG_I("Cart type %u (%s) detected.\n", number, mbcs[number]);

	/* Get ROM0 area */
	rom0_area = resource_get("rom0",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);

	/* Get boot ROM area */
	bootrom_area = resource_get("bootrom",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);

	/* Get lock area */
	lock_area = resource_get("lock",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);

	/* Map ROM0 */
	gb_mapper->rom0 = file_map(PATH_DATA,
		mach_data->cart_path,
		0,
		ROM_BANK_SIZE);
	if (!gb_mapper->rom0) {
		LOG_E("Could not map ROM0!\n");
		return false;
	}

	/* Map boot ROM */
	gb_mapper->bootrom = file_map(PATH_SYSTEM,
		mach_data->bootrom_path,
		0,
		BOOTROM_SIZE);
	if (!gb_mapper->bootrom) {
		LOG_E("Could not map boot ROM!\n");
		return false;
	}

	/* Add mapper controller */
	add_mapper(instance, number);

	/* Add ROM0 region */
	gb_mapper->rom0_region.area = rom0_area;
	gb_mapper->rom0_region.mops = &rom_mops;
	gb_mapper->rom0_region.data = gb_mapper->rom0;
	memory_region_add(&gb_mapper->rom0_region);

	/* Add boot ROM region */
	gb_mapper->bootrom_region.area = bootrom_area;
	gb_mapper->bootrom_region.mops = &rom_mops;
	gb_mapper->bootrom_region.data = gb_mapper->bootrom;
	memory_region_add(&gb_mapper->bootrom_region);

	/* Add lock region */
	gb_mapper->lock_region.area = lock_area;
	gb_mapper->lock_region.mops = &lock_mops;
	gb_mapper->lock_region.data = gb_mapper;
	memory_region_add(&gb_mapper->lock_region);

	/* Set initial locked state */
	gb_mapper->bootrom_locked = false;

	return true;
}

void gb_mapper_reset(struct controller_instance *instance)
{
	struct gb_mapper *gb_mapper = instance->priv_data;

	/* Re-add bootrom region if bootrom is locked */
	if (gb_mapper->bootrom_locked) {
		memory_region_add(&gb_mapper->bootrom_region);
		gb_mapper->bootrom_locked = false;
	}
}

void gb_mapper_deinit(struct controller_instance *instance)
{
	struct gb_mapper *gb_mapper = instance->priv_data;

	/* Unmap areas */
	file_unmap(gb_mapper->rom0, ROM_BANK_SIZE);
	file_unmap(gb_mapper->bootrom, BOOTROM_SIZE);

	/* Free allocated structures */
	free(gb_mapper->mbc_instance);
	free(gb_mapper);
}

CONTROLLER_START(gb_mapper)
	.init = gb_mapper_init,
	.reset = gb_mapper_reset,
	.deinit = gb_mapper_deinit
CONTROLLER_END

