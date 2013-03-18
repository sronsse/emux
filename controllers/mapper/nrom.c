#include <stdlib.h>
#include <controller.h>
#include <memory.h>
#include <controllers/mapper/nes_mapper.h>

#define PRG_ROM_OFFSET 0x0010

static bool nrom_init(struct controller_instance *instance);
static void nrom_deinit(struct controller_instance *instance);
static void prg_rom_map(struct controller_instance *instance);
static void prg_rom_unmap(struct controller_instance *instance);
static uint8_t prg_rom_readb(region_data_t *data, uint16_t address);
static uint16_t prg_rom_readw(region_data_t *data, uint16_t address);

struct prg_rom_data {
	uint8_t *mem;
	uint16_t size;
};

struct nrom_data {
	struct region prg_rom_region;
};

static struct mops prg_rom_mops = {
	.readb = prg_rom_readb,
	.readw = prg_rom_readw
};

uint8_t prg_rom_readb(region_data_t *data, uint16_t address)
{
	struct prg_rom_data *prg_rom_data = data;
	uint8_t *mem;

	/* Handle NROM-128 mirroring */
	address %= prg_rom_data->size;

	mem = prg_rom_data->mem + address;
	return *mem;
}

uint16_t prg_rom_readw(region_data_t *data, uint16_t address)
{
	struct prg_rom_data *prg_rom_data = data;
	uint8_t *mem;

	/* Handle NROM-128 mirroring */
	address %= prg_rom_data->size;

	mem = prg_rom_data->mem + address;
	return (*(mem + 1) << 8) | *mem;
}

void prg_rom_map(struct controller_instance *instance)
{
	struct nes_mapper_mach_data *mach_data = instance->mach_data;
	struct nrom_data *priv_data = instance->priv_data;
	struct prg_rom_data *data;
	struct region *region = &priv_data->prg_rom_region;
	struct cart_header *cart_header;
	struct resource *res;
	uint16_t prg_rom_size;

	/* Get PRG ROM resource from machine data */
	res = resource_get("prg_rom",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);

	/* Map header and compute PRG ROM size */
	cart_header = memory_map_file(mach_data->path, 0,
		sizeof(struct cart_header));
	prg_rom_size = cart_header->prg_rom_size * KB(16);
	memory_unmap_file(cart_header, sizeof(struct cart_header));

	/* Allocate and fill region data */
	data = malloc(sizeof(struct prg_rom_data));
	data->mem = memory_map_file(mach_data->path, PRG_ROM_OFFSET,
		prg_rom_size);
	data->size = prg_rom_size;

	/* Fill and add PRG ROM region */
	region->area = res;
	region->mirrors = NULL;
	region->num_mirrors = 0;
	region->mops = &prg_rom_mops;
	region->data = data;
	memory_region_add(region);
}

void prg_rom_unmap(struct controller_instance *instance)
{
	struct nrom_data *priv_data = instance->priv_data;
	struct prg_rom_data *data = priv_data->prg_rom_region.data;
	memory_unmap_file(data->mem, data->size);
	free(data);
}

bool nrom_init(struct controller_instance *instance)
{
	instance->priv_data = malloc(sizeof(struct nrom_data));
	prg_rom_map(instance);
	return true;
}

void nrom_deinit(struct controller_instance *instance)
{
	prg_rom_unmap(instance);
	free(instance->priv_data);
}

CONTROLLER_START(nrom)
	.init = nrom_init,
	.deinit = nrom_deinit
CONTROLLER_END

