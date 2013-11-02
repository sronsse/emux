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

#define MASTER_CLOCK_RATE		21477264
#define CPU_CLOCK_RATE			(MASTER_CLOCK_RATE / 12)
#define PPU_CLOCK_RATE			(MASTER_CLOCK_RATE / 4)

#define WRAM_SIZE			KB(2)
#define VRAM_SIZE			KB(2)
#define PALETTE_SIZE			32

/* CPU memory map */
#define WRAM_START			0x0000
#define WRAM_END			0x07FF
#define WRAM_MIRROR_START		0x0800
#define WRAM_MIRROR_END			0x1FFF
#define PPU_START			0x2000
#define PPU_END				0x2007
#define PPU_MIRROR_START		0x2008
#define PPU_MIRROR_END			0x3FFF
#define SPRITE_DMA_START		0x4014
#define SPRITE_DMA_END			0x4014
#define CTRL_START			0x4016
#define CTRL_END			0x4017
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
#define PALETTE_START			0x3F00
#define PALETTE_END			0x3F1F
#define PALETTE_MIRROR_START		0x3F20
#define PALETTE_MIRROR_END		0x3FFF

#define NMI_IRQ				0

enum {
	CPU_BUS_ID,
	PPU_BUS_ID
};

struct nes_data {
	uint8_t wram[WRAM_SIZE];
	uint8_t vram[VRAM_SIZE];
	uint8_t palette[PALETTE_SIZE];
};

static bool nes_init();
static void nes_deinit();
static void nes_print_usage();
static uint8_t palette_readb(region_data_t *data, address_t address);
static void palette_writeb(region_data_t *data, uint8_t b, address_t address);

/* WRAM area */
static struct resource wram_mirror =
	MEM("mem_mirror", CPU_BUS_ID, WRAM_MIRROR_START, WRAM_MIRROR_END);

static struct resource wram_area =
	MEMX("mem", CPU_BUS_ID, WRAM_START, WRAM_END, &wram_mirror, 1);

/* Palette area */
static struct resource palette_mirror =
	MEM("mem_mirror", PPU_BUS_ID, PALETTE_MIRROR_START, PALETTE_MIRROR_END);

static struct resource palette_area =
	MEMX("mem", PPU_BUS_ID, PALETTE_START, PALETTE_END, &palette_mirror, 1);

static struct mops palette_mops = {
	.readb = palette_readb,
	.writeb = palette_writeb
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

/* Sprite DMA controller */
static struct resource sprite_dma_resource =
	MEM("mem", CPU_BUS_ID, SPRITE_DMA_START, SPRITE_DMA_END);

static struct controller_instance sprite_dma_instance = {
	.controller_name = "nes_sprite",
	.bus_id = CPU_BUS_ID,
	.resources = &sprite_dma_resource,
	.num_resources = 1
};

/* NES standard controller */
static struct resource nes_controller_resource =
	MEM("mem", CPU_BUS_ID, CTRL_START, CTRL_END);

static struct controller_instance nes_controller_instance = {
	.controller_name = "nes_controller",
	.resources = &nes_controller_resource,
	.num_resources = 1
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

uint8_t palette_readb(region_data_t *data, address_t address)
{
	uint8_t *ram = data;

	/* Addresses 0x3F10, 0x3F14, 0x3F18, 0x3F1C are mirrors of
	0x3F00, 0x3F04, 0x3F08, 0x3F0C */
	switch (address) {
	case 0x10:
	case 0x14:
	case 0x18:
	case 0x1C:
		address -= 0x10;
		break;
	default:
		break;
	}

	/* Read palette entry */
	return ram[address];
}

void palette_writeb(region_data_t *data, uint8_t b, address_t address)
{
	uint8_t *ram = data;

	/* Addresses 0x3F10, 0x3F14, 0x3F18, 0x3F1C are mirrors of
	0x3F00, 0x3F04, 0x3F08, 0x3F0C */
	switch (address) {
	case 0x10:
	case 0x14:
	case 0x18:
	case 0x1C:
		address -= 0x10;
		break;
	default:
		break;
	}

	/* Read palette entry */
	ram[address] = b;
}

void nes_print_usage()
{
	fprintf(stderr, "Valid nes options:\n");
	fprintf(stderr, "  --cart    Game cart path\n");
}

bool nes_init(struct machine *machine)
{
	struct nes_data *nes_data;

	/* Create machine data structure */
	nes_data = malloc(sizeof(struct nes_data));

	/* Get cart option */
	if (!cmdline_parse_string("cart", &nes_mapper_mach_data.path)) {
		free(nes_data);
		fprintf(stderr, "Please provide a cart option!\n");
		nes_print_usage();
		return false;
	}

	/* Add memory busses */
	memory_bus_add(16); /* CPU bus */
	memory_bus_add(16); /* PPU bus */

	/* Add memory regions */
	memory_region_add(&wram_area, &ram_mops, nes_data->wram);
	memory_region_add(&palette_area, &palette_mops, nes_data->palette);

	/* NES cart controls VRAM address lines so let the mapper handle it */
	nes_mapper_mach_data.vram = nes_data->vram;

	/* Add controllers and CPU */
	if (!controller_add(&sprite_dma_instance) ||
		!controller_add(&nes_mapper_instance) ||
		!controller_add(&ppu_instance) ||
		!controller_add(&nes_controller_instance) ||
		!cpu_add(&rp2a03_instance)) {
		free(nes_data);
		return false;
	}

	/* Save machine data structure */
	machine->priv_data = nes_data;

	return true;
}

void nes_deinit(struct machine *machine)
{
	free(machine->priv_data);
}

MACHINE_START(nes, "Nintendo Entertainment System")
	.init = nes_init,
	.deinit = nes_deinit
MACHINE_END

