#include <stdlib.h>
#include <controller.h>
#include <file.h>
#include <memory.h>
#include <controllers/mapper/nes_mapper.h>

struct nrom {
	uint8_t *vram;
	uint8_t *prg_rom;
	uint8_t *chr_rom;
	int prg_rom_size;
	int chr_rom_size;
	bool vertical_mirroring;
	struct region vram_region;
	struct region prg_rom_region;
	struct region chr_rom_region;
};

static bool nrom_init(struct controller_instance *instance);
static void nrom_deinit(struct controller_instance *instance);
static uint8_t vram_readb(struct nrom *nrom, address_t address);
static uint16_t vram_readw(struct nrom *nrom, address_t address);
static void vram_writeb(struct nrom *nrom, uint8_t b, address_t address);
static void vram_writew(struct nrom *nrom, uint16_t w, address_t address);
static void mirror_address(struct nrom *nrom, address_t *address);
static uint8_t prg_rom_readb(struct nrom *nrom, address_t address);
static uint16_t prg_rom_readw(struct nrom *nrom, address_t address);

static struct mops vram_mops = {
	.readb = (readb_t)vram_readb,
	.readw = (readw_t)vram_readw,
	.writeb = (writeb_t)vram_writeb,
	.writew = (writew_t)vram_writew
};

static struct mops prg_rom_mops = {
	.readb = (readb_t)prg_rom_readb,
	.readw = (readw_t)prg_rom_readw
};

uint8_t vram_readb(struct nrom *nrom, address_t address)
{
	mirror_address(nrom, &address);
	return ram_mops.readb(nrom->vram, address);
}

uint16_t vram_readw(struct nrom *nrom, address_t address)
{
	mirror_address(nrom, &address);
	return ram_mops.readw(nrom->vram, address);
}

void vram_writeb(struct nrom *nrom, uint8_t b, address_t address)
{
	mirror_address(nrom, &address);
	ram_mops.writeb(nrom->vram, b, address);
}

void vram_writew(struct nrom *nrom, uint16_t w, address_t address)
{
	mirror_address(nrom, &address);
	ram_mops.writew(nrom->vram, w, address);
}

void mirror_address(struct nrom *nrom, address_t *address)
{
	bool bit;

	/* The NES hardware lets the cart control some VRAM lines as follows:
	Vertical mirroring: $2000 equals $2800 and $2400 equals $2C00
	Horizontal mirroring: $2000 equals $2400 and $2800 equals $2C00 */

	/* Adapt address in function of selected mirroring */
	if (nrom->vertical_mirroring) {
		/* Clear bit 11 of address */
		*address &= ~(1 << 11);
	} else {
		/* Set bit 10 of address to bit 11 and clear bit 11 */
		bit = *address & (1 << 11);
		*address &= ~(1 << 10);
		*address |= (bit << 10);
		*address &= ~(bit << 11);
	}
}

uint8_t prg_rom_readb(struct nrom *nrom, address_t address)
{
	/* Handle NROM-128 mirroring */
	address %= nrom->prg_rom_size;

	return *(nrom->prg_rom + address);
}

uint16_t prg_rom_readw(struct nrom *nrom, address_t address)
{
	uint8_t *mem;

	/* Handle NROM-128 mirroring */
	address %= nrom->prg_rom_size;

	mem = nrom->prg_rom + address;
	return (*(mem + 1) << 8) | *mem;
}

bool nrom_init(struct controller_instance *instance)
{
	struct nrom *nrom;
	struct nes_mapper_mach_data *mach_data = instance->mach_data;
	struct cart_header *cart_header;
	struct resource *area;

	/* Allocate NROM structure */
	instance->priv_data = malloc(sizeof(struct nrom));
	nrom = instance->priv_data;

	/* Map cart header */
	cart_header = file_map(PATH_DATA, mach_data->path, 0,
		sizeof(struct cart_header));

	/* Get mirroring information (used for VRAM access) - NROM supports
	only horizontal and vertical mirroring */
	nrom->vertical_mirroring = (cart_header->flags6 & 0x09);

	/* Save VRAM (specified by machine) */
	nrom->vram = mach_data->vram;

	/* Add VRAM region */
	area = resource_get("vram",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	nrom->vram_region.area = area;
	nrom->vram_region.mops = &vram_mops;
	nrom->vram_region.data = nrom;
	memory_region_add(&nrom->vram_region);

	/* Allocate and fill PRG ROM data */
	nrom->prg_rom_size = PRG_ROM_SIZE(cart_header);
	nrom->prg_rom = file_map(PATH_DATA, mach_data->path,
		PRG_ROM_OFFSET(cart_header), nrom->prg_rom_size);

	/* Fill and add PRG ROM region */
	area = resource_get("prg_rom",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	nrom->prg_rom_region.area = area;
	nrom->prg_rom_region.mops = &prg_rom_mops;
	nrom->prg_rom_region.data = nrom;
	memory_region_add(&nrom->prg_rom_region);

	/* Allocate and fill CHR ROM data */
	nrom->chr_rom_size = CHR_ROM_SIZE(cart_header);
	nrom->chr_rom = file_map(PATH_DATA, mach_data->path,
		CHR_ROM_OFFSET(cart_header), nrom->chr_rom_size);

	/* Fill and add PRG ROM region */
	area = resource_get("chr",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	nrom->chr_rom_region.area = area;
	nrom->chr_rom_region.mops = &rom_mops;
	nrom->chr_rom_region.data = nrom->chr_rom;
	memory_region_add(&nrom->chr_rom_region);

	/* Unmap cart header */
	file_unmap(cart_header, sizeof(struct cart_header));

	return true;
}

void nrom_deinit(struct controller_instance *instance)
{
	struct nrom *nrom = instance->priv_data;
	file_unmap(nrom->chr_rom, nrom->chr_rom_size);
	file_unmap(nrom->prg_rom, nrom->prg_rom_size);
	free(nrom);
}

CONTROLLER_START(nrom)
	.init = nrom_init,
	.deinit = nrom_deinit
CONTROLLER_END

