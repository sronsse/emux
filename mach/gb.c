#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmdline.h>
#include <controller.h>
#include <cpu.h>
#include <env.h>
#include <log.h>
#include <machine.h>
#include <memory.h>
#include <util.h>
#include <controllers/mapper/gb_mapper.h>

#define GB_CLOCK_RATE	4194304

#define VRAM_SIZE	KB(8)
#define WRAM_SIZE	KB(8)
#define OAM_SIZE	160
#define HRAM_SIZE	127

/* Memory map */
#define BOOTROM_START	0x0000
#define BOOTROM_END	0x00FF
#define ROM0_START	0x0000
#define ROM0_END	0x3FFF
#define ROM1_START	0x4000
#define ROM1_END	0x7FFF
#define VRAM_START	0x8000
#define VRAM_END	0x9FFF
#define EXTRAM_START	0xA000
#define EXTRAM_END	0xBFFF
#define WRAM_START	0xC000
#define WRAM_END	0xDFFF
#define ECHO_START	0xE000
#define ECHO_END	0xFDFF
#define OAM_START	0xFE00
#define OAM_END		0xFE9F
#define IFR		0xFF0F
#define LCDC_START	0xFF40
#define LCDC_END	0xFF4B
#define BOOT_LOCK	0xFF50
#define HRAM_START	0xFF80
#define HRAM_END	0xFFFE
#define IER		0xFFFF

#define BUS_ID		0

#define VBLANK_IRQ	0
#define LCDC_IRQ	1
#define TIMER_IRQ	2
#define SERIAL_IRQ	3
#define JOYPAD_IRQ	4

struct gb_data {
	uint8_t vram[VRAM_SIZE];
	uint8_t wram[WRAM_SIZE];
	uint8_t oam[OAM_SIZE];
	uint8_t hram[HRAM_SIZE];
};

static bool gb_init();
static void gb_deinit();

/* Command-line parameters */
static char *bootrom_path = "DMG_ROM.bin";
PARAM(bootrom_path, string, "bootrom", "gb", "GameBoy boot ROM path")

/* Memory areas */
static struct resource vram_area = MEM("vram", BUS_ID, VRAM_START, VRAM_END);
static struct resource wram_area = MEM("wram", BUS_ID, WRAM_START, WRAM_END);
static struct resource oam_area = MEM("oam", BUS_ID, OAM_START, OAM_END);
static struct resource hram_area = MEM("hram", BUS_ID, HRAM_START, HRAM_END);

/* LR35902 CPU */
static struct resource cpu_resources[] = {
	CLK("clk", GB_CLOCK_RATE),
	MEM("ifr", BUS_ID, IFR, IFR),
	MEM("ier", BUS_ID, IER, IER)
};

static struct cpu_instance cpu_instance = {
	.cpu_name = "lr35902",
	.bus_id = BUS_ID,
	.resources = cpu_resources,
	.num_resources = ARRAY_SIZE(cpu_resources)
};

/* GB mapper controller */
static struct gb_mapper_mach_data gb_mapper_mach_data;

static struct resource gb_mapper_resources[] = {
	MEM("bootrom", BUS_ID, BOOTROM_START, BOOTROM_END),
	MEM("rom0", BUS_ID, ROM0_START, ROM0_END),
	MEM("rom1", BUS_ID, ROM1_START, ROM1_END),
	MEM("lock", BUS_ID, BOOT_LOCK, BOOT_LOCK)
};

static struct controller_instance gb_mapper_instance = {
	.controller_name = "gb_mapper",
	.bus_id = BUS_ID,
	.resources = gb_mapper_resources,
	.num_resources = ARRAY_SIZE(gb_mapper_resources),
	.mach_data = &gb_mapper_mach_data
};

/* LCD controller */
static struct resource lcdc_resources[] = {
	MEM("mem", BUS_ID, LCDC_START, LCDC_END),
	CLK("clk", GB_CLOCK_RATE),
	IRQ("vblank", VBLANK_IRQ),
	IRQ("lcdc", LCDC_IRQ)
};

static struct controller_instance lcdc_instance = {
	.controller_name = "lcdc",
	.bus_id = BUS_ID,
	.resources = lcdc_resources,
	.num_resources = ARRAY_SIZE(lcdc_resources)
};

bool gb_init(struct machine *machine)
{
	struct gb_data *gb_data;

	/* Create machine data structure */
	gb_data = malloc(sizeof(struct gb_data));

	/* Add 16-bit memory bus */
	memory_bus_add(16);

	/* Add memory regions */
	memory_region_add(&vram_area, &ram_mops, gb_data->vram);
	memory_region_add(&wram_area, &ram_mops, gb_data->wram);
	memory_region_add(&hram_area, &ram_mops, gb_data->hram);
	memory_region_add(&oam_area, &ram_mops, gb_data->oam);

	/* Set GB mapper controller machine data */
	gb_mapper_mach_data.bootrom_path = bootrom_path;
	gb_mapper_mach_data.cart_path = env_get_data_path();

	/* Add controllers and CPU */
	if (!controller_add(&gb_mapper_instance) ||
		!controller_add(&lcdc_instance) ||
		!cpu_add(&cpu_instance)) {
		free(gb_data);
		return false;
	}

	/* Save machine data structure */
	machine->priv_data = gb_data;

	return true;
}

void gb_deinit(struct machine *machine)
{
	free(machine->priv_data);
}

MACHINE_START(gb, "Nintendo Game Boy")
	.init = gb_init,
	.deinit = gb_deinit
MACHINE_END

