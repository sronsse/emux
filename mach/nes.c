#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmdline.h>
#include <controller.h>
#include <cpu.h>
#include <machine.h>
#include <memory.h>
#include <resource.h>
#include <util.h>
#include <controllers/mapper/nes_mapper.h>

#define CPU_BUS_ID			0
#define PPU_BUS_ID			1

#define NMI_IRQ				0

#define MASTER_CLOCK_RATE		21477264
#define CPU_CLOCK_RATE			(MASTER_CLOCK_RATE / 12)
#define PPU_CLOCK_RATE			(MASTER_CLOCK_RATE / 4)

#define WORK_RAM_START			0x0000
#define WORK_RAM_END			0x07FF
#define WORK_RAM_MIRROR_START		0x0800
#define WORK_RAM_MIRROR_END		0x1FFF
#define PPU_START			0x2000
#define PPU_END				0x2007
#define PPU_MIRROR_START		0x2008
#define PPU_MIRROR_END			0x3FFF
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

static struct resource rp2a03_resources[] = {
	{
		.name = "nmi",
		.irq = NMI_IRQ,
		.type = RESOURCE_IRQ
	},
	{
		.name = "clk",
		.rate = CPU_CLOCK_RATE,
		.type = RESOURCE_CLK
	}
};

static struct cpu_instance rp2a03_instance = {
	.cpu_name = "rp2a03",
	.bus_id = CPU_BUS_ID,
	.resources = rp2a03_resources,
	.num_resources = ARRAY_SIZE(rp2a03_resources)
};

static struct resource ppu_mirror = {
	.mem = {
		.bus_id = CPU_BUS_ID,
		.start = PPU_MIRROR_START,
		.end = PPU_MIRROR_END
	},
	.type = RESOURCE_MEM
};

static struct resource ppu_resources[] = {
	{
		.name = "mem",
		.mem = {
			.bus_id = CPU_BUS_ID,
			.start = PPU_START,
			.end = PPU_END
		},
		.type = RESOURCE_MEM,
		.children = &ppu_mirror,
		.num_children = 1
	},
	{
		.name = "irq",
		.irq = NMI_IRQ,
		.type = RESOURCE_IRQ
	},
	{
		.name = "clk",
		.rate = PPU_CLOCK_RATE,
		.type = RESOURCE_CLK
	}
};

static struct controller_instance ppu_instance = {
	.controller_name = "ppu",
	.bus_id = PPU_BUS_ID,
	.resources = ppu_resources,
	.num_resources = ARRAY_SIZE(ppu_resources)
};

static struct nes_mapper_mach_data nes_mapper_mach_data;

static struct resource nes_mapper_resources[] = {
	{
		.name = "expansion",
		.mem = {
			.bus_id = CPU_BUS_ID,
			.start = CART_EXPANSION_AREA_START,
			.end = CART_EXPANSION_AREA_END
		},
		.type = RESOURCE_MEM
	},
	{
		.name = "sram",
		.mem = {
			.bus_id = CPU_BUS_ID,
			.start = CART_SRAM_AREA_START,
			.end = CART_SRAM_AREA_END
		},
		.type = RESOURCE_MEM
	},
	{
		.name = "prg_rom",
		.mem = {
			.bus_id = CPU_BUS_ID,
			.start = CART_PRG_ROM_AREA_START,
			.end = CART_PRG_ROM_AREA_END
		},
		.type = RESOURCE_MEM
	}
};

static struct controller_instance nes_mapper_instance = {
	.controller_name = "nes_mapper",
	.resources = nes_mapper_resources,
	.num_resources = ARRAY_SIZE(nes_mapper_resources),
	.mach_data = &nes_mapper_mach_data
};

static struct resource wram_mirror = {
	.mem = {
		.bus_id = CPU_BUS_ID,
		.start = WORK_RAM_MIRROR_START,
		.end = WORK_RAM_MIRROR_END
	},
	.type = RESOURCE_MEM
};

static struct resource wram_area = {
	.name = "wram",
	.mem = {
		.bus_id = CPU_BUS_ID,
		.start = WORK_RAM_START,
		.end = WORK_RAM_END
	},
	.type = RESOURCE_MEM,
	.children = &wram_mirror,
	.num_children = 1
};

static struct mops wram_mops = {
	.readb = wram_readb,
	.readw = wram_readw,
	.writeb = wram_writeb,
	.writew = wram_writew
};

static struct region wram_region = {
	.area = &wram_area,
	.mops = &wram_mops
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
	fprintf(stderr, "  --cart    Game cart path\n");
}

bool nes_init()
{
	/* Get cart option */
	if (!cmdline_parse_string("cart", &nes_mapper_mach_data.path)) {
		fprintf(stderr, "Please provide a cart option!\n");
		nes_print_usage();
		return false;
	}

	/* Add work ram region */
	wram_region.data = malloc(WORK_RAM_END - WORK_RAM_START + 1);
	memory_region_add(&wram_region);

	/* Add controllers */
	if (!controller_add(&nes_mapper_instance))
		return false;
	if (!controller_add(&ppu_instance))
		return false;

	/* Add main CPU */
	if (!cpu_add(&rp2a03_instance))
		return false;

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

