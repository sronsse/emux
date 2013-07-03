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

#define WRAM_SIZE			KB(2)
#define VRAM_SIZE			KB(2)

/* CPU memory map */
#define WRAM_START			0x0000
#define WRAM_END			0x07FF
#define WRAM_MIRROR_START		0x0800
#define WRAM_MIRROR_END			0x1FFF
#define PPU_START			0x2000
#define PPU_END				0x2007
#define PPU_MIRROR_START		0x2008
#define PPU_MIRROR_END			0x3FFF
#define EXPANSION_START			0x4018
#define EXPANSION_END			0x5FFF
#define SRAM_START			0x6000
#define SRAM_END			0x7FFF
#define PRG_ROM_START			0x8000
#define PRG_ROM_END			0xFFFF

/* PPU memory map */
#define CHR_START			0x0000
#define CHR_END				0x1FFF
#define VRAM_START			0x2000
#define VRAM_END			0x2FFF
#define VRAM_MIRROR_START		0x3000
#define VRAM_MIRROR_END			0x3EFF

static bool nes_init();
static void nes_deinit();
static void nes_print_usage();

/* Internal memory */
static uint8_t wram[WRAM_SIZE];
static uint8_t vram[VRAM_SIZE];

/* WRAM area */
static struct resource wram_mirror =
	MEM("mem_mirror", CPU_BUS_ID, WRAM_MIRROR_START, WRAM_MIRROR_END);

static struct resource wram_area =
	MEMX("mem", CPU_BUS_ID, WRAM_START, WRAM_END, &wram_mirror, 1);

static struct region wram_region = {
	.area = &wram_area,
	.mops = &ram_mops,
	.data = wram
};

/* RP2A03 CPU */
static struct resource rp2a03_resources[] = {
	IRQ("nmi", NMI_IRQ),
	CLK("clk", CPU_CLOCK_RATE)
};

static struct cpu_instance rp2a03_instance = {
	.cpu_name = "rp2a03",
	.bus_id = CPU_BUS_ID,
	.resources = rp2a03_resources,
	.num_resources = ARRAY_SIZE(rp2a03_resources)
};

/* PPU controller */
static struct resource ppu_mirror =
	MEM("mem_mirror", CPU_BUS_ID, PPU_MIRROR_START, PPU_MIRROR_END);

static struct resource ppu_resources[] = {
	MEMX("mem", CPU_BUS_ID, PPU_START, PPU_END, &ppu_mirror, 1),
	IRQ("irq", NMI_IRQ),
	CLK("clk", PPU_CLOCK_RATE)
};

static struct controller_instance ppu_instance = {
	.controller_name = "ppu",
	.bus_id = PPU_BUS_ID,
	.resources = ppu_resources,
	.num_resources = ARRAY_SIZE(ppu_resources)
};

/* NES mapper controller */
static struct nes_mapper_mach_data nes_mapper_mach_data;

static struct resource vram_mirror =
	MEM("vram", PPU_BUS_ID, VRAM_MIRROR_START, VRAM_MIRROR_END);

static struct resource nes_mapper_resources[] = {
	MEM("expansion", CPU_BUS_ID, EXPANSION_START, EXPANSION_END),
	MEM("sram", CPU_BUS_ID, SRAM_START, SRAM_END),
	MEM("prg_rom", CPU_BUS_ID, PRG_ROM_START, PRG_ROM_END),
	MEM("chr", PPU_BUS_ID, CHR_START, CHR_END),
	MEMX("vram", PPU_BUS_ID, VRAM_START, VRAM_END, &vram_mirror, 1)
};

static struct controller_instance nes_mapper_instance = {
	.controller_name = "nes_mapper",
	.resources = nes_mapper_resources,
	.num_resources = ARRAY_SIZE(nes_mapper_resources),
	.mach_data = &nes_mapper_mach_data
};

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
	memory_region_add(&wram_region);

	/* NES cart controls VRAM address lines so let the mapper handle it */
	nes_mapper_mach_data.vram = vram;

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
}

MACHINE_START(nes, "Nintendo Entertainment System")
	.init = nes_init,
	.deinit = nes_deinit
MACHINE_END

