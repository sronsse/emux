#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmdline.h>
#include <machine.h>
#include <memory.h>
#include <resource.h>
#include <util.h>
#include <controllers/mapper/nes_mapper.h>

#define WORK_RAM_START			0x0000
#define WORK_RAM_END			0x07FF
#define WORK_RAM_MIRROR_START		0x0800
#define WORK_RAM_MIRROR_END		0x1FFF
#define CART_EXPANSION_AREA_START	0x4018
#define CART_EXPANSION_AREA_END		0x5FFF
#define CART_SRAM_AREA_START		0x6000
#define CART_SRAM_AREA_END		0x7FFF
#define CART_PRG_ROM_AREA_START		0x8000
#define CART_PRG_ROM_AREA_END		0xFFFF

static bool nes_init();
static void nes_deinit();
static void nes_print_usage();
static uint8_t wram_readb(region_data_t *data, uint16_t address);
static uint16_t wram_readw(region_data_t *data, uint16_t address);
static void wram_writeb(region_data_t *data, uint8_t b, uint16_t address);
static void wram_writew(region_data_t *data, uint16_t w, uint16_t address);

static struct resource wram_area = {
	.name = "wram",
	.start = WORK_RAM_START,
	.end = WORK_RAM_END,
	.type = RESOURCE_MEM
};

static struct resource wram_mirror = {
	.name = "wram_mirror",
	.start = WORK_RAM_MIRROR_START,
	.end = WORK_RAM_MIRROR_END,
	.type = RESOURCE_MEM
};

static struct mops wram_mops = {
	.readb = wram_readb,
	.readw = wram_readw,
	.writeb = wram_writeb,
	.writew = wram_writew
};

static struct region wram_region = {
	.area = &wram_area,
	.mirrors = &wram_mirror,
	.num_mirrors = 1,
	.mops = &wram_mops
};

static struct controller_instance ppu_instance = {
	.controller_name = "ppu"
};

static struct nes_mapper_mach_data nes_mapper_mach_data;

static struct resource nes_mapper_resources[] = {
	{
		.name = "expansion",
		.start = CART_EXPANSION_AREA_START,
		.end = CART_EXPANSION_AREA_END,
		.type = RESOURCE_MEM
	},
	{
		.name = "sram",
		.start = CART_SRAM_AREA_START,
		.end = CART_SRAM_AREA_END,
		.type = RESOURCE_MEM
	},
	{
		.name = "prg_rom",
		.start = CART_PRG_ROM_AREA_START,
		.end = CART_PRG_ROM_AREA_END,
		.type = RESOURCE_MEM
	}
};

static struct controller_instance nes_mapper_instance = {
	.controller_name = "nes_mapper",
	.resources = nes_mapper_resources,
	.num_resources = ARRAY_SIZE(nes_mapper_resources),
	.mach_data = &nes_mapper_mach_data
};

uint8_t wram_readb(region_data_t *data, uint16_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	return *mem;
}

uint16_t wram_readw(region_data_t *data, uint16_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	return (*(mem + 1) << 8) | *mem;
}

void wram_writeb(region_data_t *data, uint8_t b, uint16_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	*mem = b;
}

void wram_writew(region_data_t *data, uint16_t w, uint16_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	*mem++ = w;
	*mem = w >> 8;
}

void nes_print_usage()
{
	fprintf(stderr, "Valid nes options:\n");
	fprintf(stderr, "  -c, --cart    Game cart path\n");
}

bool nes_init()
{
	/* Get cart option */
	if (!cmdline_parse_string("cart", 'c', &nes_mapper_mach_data.path)) {
		fprintf(stderr, "Please provide a cart option!\n");
		nes_print_usage();
		return false;
	}

	/* Add work ram region */
	wram_region.data = malloc(WORK_RAM_END - WORK_RAM_START + 1);
	memory_region_add(&wram_region);

	/* Add CPU and controllers */
	cpu_add("rp2a03");
	controller_add(&ppu_instance);
	controller_add(&nes_mapper_instance);

	return true;
}

void nes_deinit()
{
	free(wram_region.data);
}

MACHINE_START(nes, "Nintendo Entertainment System")
	.init = nes_init,
	.deinit = nes_deinit
MACHINE_END

