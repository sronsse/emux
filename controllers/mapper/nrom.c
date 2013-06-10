#include <stdlib.h>
#include <controller.h>
#include <memory.h>
#include <controllers/mapper/nes_mapper.h>

#define PRG_ROM_OFFSET 0x0010

static void prg_rom_map(struct controller *controller);
static void prg_rom_unmap(struct controller *controller);
static uint8_t prg_rom_readb(struct region *region, uint16_t address);
static uint16_t prg_rom_readw(struct region *region, uint16_t address);
static void prg_rom_writeb(struct region *region, uint8_t b, uint16_t address);
static void prg_rom_writew(struct region *region, uint16_t w, uint16_t address);

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

uint8_t prg_rom_readb(struct region *region, uint16_t address)
{
	struct prg_rom_data *data = region->data;
	uint8_t *mem;

	/* Handle NROM-128 mirroring */
	address %= data->size;

	mem = data->mem + address;
	return *mem;
}

uint16_t prg_rom_readw(struct region *region, uint16_t address)
{
	struct prg_rom_data *data = region->data;
	uint8_t *mem;

	/* Handle NROM-128 mirroring */
	address %= data->size;

	mem = data->mem + address;
	return (*(mem + 1) << 8) | *mem;
}

void prg_rom_map(struct controller *controller)
{
	struct nes_mapper_mach_data *mdata = controller->mdata;
	struct nrom_data *cdata = controller->cdata;
	struct prg_rom_data *data;
	struct region *region = &cdata->prg_rom_region;
	struct cart_header *cart_header;
	struct resource *res;
	uint16_t prg_rom_size;

	/* Get PRG ROM resource from machine data */
	res = resource_get("prg_rom",
		RESOURCE_MEM,
		mdata->resources,
		mdata->num_resources);

	/* Map header and compute PRG ROM size */
	cart_header = memory_map_file(mdata->path, 0, sizeof(struct cart_header));
	prg_rom_size = cart_header->prg_rom_size * KB(16);
	memory_unmap_file(cart_header, sizeof(struct cart_header));

	/* Allocate and fill region data */
	data = malloc(sizeof(struct prg_rom_data));
	data->mem = memory_map_file(mdata->path, PRG_ROM_OFFSET, prg_rom_size);
	data->size = prg_rom_size;

	/* Fill and add PRG ROM region */
	region->area = res;
	region->mirrors = NULL;
	region->num_mirrors = 0;
	region->mops = &prg_rom_mops;
	region->data = data;
	memory_region_add(region);
}

void prg_rom_unmap(struct controller *controller)
{
	struct nrom_data *cdata = controller->cdata;
	struct prg_rom_data *data = cdata->prg_rom_region.data;
	memory_unmap_file(data->mem, data->size);
	free(data);
}

bool nrom_init(struct controller *controller)
{
	controller->cdata = malloc(sizeof(struct nrom_data));
	prg_rom_map(controller);
	return true;
}

void nrom_deinit(struct controller *controller)
{
	prg_rom_unmap(controller);
	free(controller->cdata);
}

CONTROLLER_START(nrom)
	.init = nrom_init,
	.deinit = nrom_deinit
CONTROLLER_END

