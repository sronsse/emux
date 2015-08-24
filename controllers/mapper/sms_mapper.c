#include <stdlib.h>
#include <controller.h>
#include <file.h>
#include <log.h>
#include <memory.h>
#include <port.h>
#include <util.h>
#include <controllers/mapper/sms_mapper.h>

#define BIOS_SIZE	0x2000
#define HEADER_START	0x7FF0

struct cart_header {
	char signature[8];
	uint8_t reserved[2];
	uint8_t checksum[2];
	uint16_t product_low;
	struct {
		uint8_t product_high:4;
		uint8_t version:4;
	};
	struct {
		uint8_t rom_size:4;
		uint8_t region:4;
	};
};

union slot_control {
	uint8_t raw;
	struct {
		uint8_t unknown:2;
		uint8_t io_disable:1;
		uint8_t bios_disable:1;
		uint8_t ram_disable:1;
		uint8_t card_disable:1;
		uint8_t cart_disable:1;
		uint8_t exp_disable:1;
	};
};

struct sms_mapper {
	uint8_t *bios;
	struct region bios_region;
	struct region cart_region;
	struct port_region port_region;
	struct controller_instance mapper_instance;
	struct cart_mapper_mach_data mapper_data;
	union slot_control slot_control;
};

static bool sms_mapper_init(struct controller_instance *instance);
static void sms_mapper_reset(struct controller_instance *instance);
static void sms_mapper_deinit(struct controller_instance *instance);
static void print_header(char *cart_path);
static void add_mapper(struct controller_instance *instance);
static void ctrl_writeb(struct sms_mapper *sms_mapper, uint8_t b);

static struct pops ctrl_pops = {
	.write = (write_t)ctrl_writeb
};

void ctrl_writeb(struct sms_mapper *sms_mapper, uint8_t b)
{
	union slot_control prev;

	/* Save previous slot control register */
	prev = sms_mapper->slot_control;

	/* Set slot control with port data */
	sms_mapper->slot_control.raw = b;

	/* Add/remove BIOS region */
	if (prev.bios_disable && !sms_mapper->slot_control.bios_disable)
		memory_region_add(&sms_mapper->bios_region);
	else if (!prev.bios_disable && sms_mapper->slot_control.bios_disable)
		memory_region_remove(&sms_mapper->bios_region);

	/* Add/remove cart region */
	if (prev.cart_disable && !sms_mapper->slot_control.cart_disable)
		memory_region_add(&sms_mapper->cart_region);
	else if (!prev.cart_disable && sms_mapper->slot_control.cart_disable)
		memory_region_remove(&sms_mapper->cart_region);
}

void print_header(char *cart_path)
{
	file_handle_t file;
	uint32_t size;
	struct cart_header h;

	/* Open cart file */
	file = file_open(PATH_DATA, cart_path, "rb");
	if (!file) {
		LOG_W("Could not open cart!\n");
		return;
	}

	/* Validate cart size */
	size = file_get_size(file);
	if (size < HEADER_START + sizeof(struct cart_header)) {
		LOG_E("Could not read cart header!\n");
		return;
	}

	/* Read cart header */
	file_read(file, &h, HEADER_START, sizeof(struct cart_header));

	/* Close cart file */
	file_close(file);

	/* Print cart header */
	LOG_I("Signature: %.*s\n", ARRAY_SIZE(h.signature), h.signature);
	LOG_I("Checksum: %04x\n", h.checksum);
	LOG_I("Product: %03x\n", (h.product_high << 8) | (h.product_low));
	LOG_I("Version: %01x\n", h.version);
	LOG_I("Region: %01x\n", h.region);
	LOG_I("ROM size: %01x\n", h.rom_size);
}

void add_mapper(struct controller_instance *instance)
{
	struct sms_mapper *sms_mapper = instance->priv_data;
	struct sms_mapper_mach_data *mach_data = instance->mach_data;
	struct controller_instance *mapper = &sms_mapper->mapper_instance;
	struct cart_mapper_mach_data *data = &sms_mapper->mapper_data;

	/* Initialize SEGA mapper (currently the only one supported) */
	mapper->controller_name = "sega_mapper";
	mapper->bus_id = instance->bus_id;
	mapper->num_resources = instance->num_resources;
	mapper->resources = instance->resources;

	/* Set mapper machine data */
	data->cart_path = mach_data->cart_path;
	data->cart_region = &sms_mapper->cart_region;
	mapper->mach_data = data;

	/* Add mapper controller */
	controller_add(mapper);
}

bool sms_mapper_init(struct controller_instance *instance)
{
	struct sms_mapper *sms_mapper;
	struct sms_mapper_mach_data *mach_data;
	struct resource *res;

	/* Allocate SMS mapper structure */
	instance->priv_data = calloc(1, sizeof(struct sms_mapper));
	sms_mapper = instance->priv_data;

	/* Get mach data from instance */
	mach_data = instance->mach_data;

	/* Print cart information */
	print_header(mach_data->cart_path);

	/* Map BIOS */
	sms_mapper->bios = file_map(PATH_SYSTEM,
		mach_data->bios_path,
		0,
		BIOS_SIZE);
	if (!sms_mapper->bios) {
		LOG_E("Could not map BIOS!\n");
		return false;
	}

	/* Initialize BIOS region */
	res = resource_get("mapper",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	sms_mapper->bios_region.area = res;
	sms_mapper->bios_region.mops = &rom_mops;
	sms_mapper->bios_region.data = sms_mapper->bios;

	/* Add control port region */
	res = resource_get("port",
		RESOURCE_PORT,
		instance->resources,
		instance->num_resources);
	sms_mapper->port_region.area = res;
	sms_mapper->port_region.pops = &ctrl_pops;
	sms_mapper->port_region.data = sms_mapper;
	port_region_add(&sms_mapper->port_region);

	/* Initialize slot control register (disabling all slots) */
	sms_mapper->slot_control.raw = 0xFF;

	/* Add actual mapper controller */
	add_mapper(instance);

	return true;
}

void sms_mapper_reset(struct controller_instance *instance)
{
	struct sms_mapper *sms_mapper = instance->priv_data;

	/* Remove BIOS region if needed */
	if (!sms_mapper->slot_control.bios_disable)
		memory_region_remove(&sms_mapper->bios_region);

	/* Reset slot control register and enable BIOS */
	sms_mapper->slot_control.raw = 0xFF;
	sms_mapper->slot_control.bios_disable = 0;

	/* Add BIOS region */
	memory_region_add(&sms_mapper->bios_region);
}

void sms_mapper_deinit(struct controller_instance *instance)
{
	struct sms_mapper *sms_mapper = instance->priv_data;

	/* Unmap BIOS */
	file_unmap(sms_mapper->bios, BIOS_SIZE);

	/* Free private data */
	free(sms_mapper);
}

CONTROLLER_START(sms_mapper)
	.init = sms_mapper_init,
	.reset = sms_mapper_reset,
	.deinit = sms_mapper_deinit
CONTROLLER_END

