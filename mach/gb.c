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

/* Clock frequencies */
#define GB_CLOCK_RATE		4194304
#define SOUND_CLOCK_RATE	GB_CLOCK_RATE
#define SERIAL_CLOCK_RATE	(GB_CLOCK_RATE / 512)
#define DIV_CLOCK_RATE		(GB_CLOCK_RATE / 256)
#define TIMA_CLOCK_RATE		GB_CLOCK_RATE

/* Interrupt definitions */
#define VBLANK_IRQ		0
#define LCDC_IRQ		1
#define TIMER_IRQ		2
#define SERIAL_IRQ		3
#define JOYPAD_IRQ		4

/* Bus definitions */
#define BUS_ID			0

/* Memory sizes */
#define VRAM_SIZE		KB(8)
#define WRAM_SIZE		KB(8)
#define HRAM_SIZE		127
#define OAM_SIZE		160
#define WAVE_SIZE		16

/* Memory map */
#define BOOTROM_START		0x0000
#define BOOTROM_END		0x00FF
#define ROM0_START		0x0000
#define ROM0_END		0x3FFF
#define ROM1_START		0x4000
#define ROM1_END		0x7FFF
#define VRAM_START		0x8000
#define VRAM_END		0x9FFF
#define EXTRAM_START		0xA000
#define EXTRAM_END		0xBFFF
#define WRAM_START		0xC000
#define WRAM_END		0xDFFF
#define ECHO_START		0xE000
#define ECHO_END		0xFDFF
#define OAM_START		0xFE00
#define OAM_END			0xFE9F
#define JOYPAD			0xFF00
#define SERIAL_START		0xFF01
#define SERIAL_END		0xFF02
#define TIMER_START		0xFF04
#define TIMER_END		0xFF07
#define IFR			0xFF0F
#define SOUND_START		0xFF10
#define SOUND_END		0xFF26
#define WAVE_START		0xFF30
#define WAVE_END		0xFF3F
#define LCDC_START		0xFF40
#define LCDC_END		0xFF4B
#define BOOT_LOCK		0xFF50
#define HRAM_START		0xFF80
#define HRAM_END		0xFFFE
#define IER			0xFFFF

struct gb_data {
	uint8_t vram[VRAM_SIZE];
	uint8_t wram[WRAM_SIZE];
	uint8_t hram[HRAM_SIZE];
	uint8_t oam[OAM_SIZE];
	uint8_t wave[WAVE_SIZE];
	struct bus bus;
	struct region vram_region;
	struct region wram_region;
	struct region hram_region;
	struct region oam_region;
	struct region wave_region;
};

static bool gb_init();
static void gb_deinit();

/* Command-line parameters */
static char *bootrom_path = "DMG_ROM.bin";
PARAM(bootrom_path, string, "bootrom", "gb", "GameBoy boot ROM path")

/* VRAM area */
static struct resource vram_area =
	MEM("vram", BUS_ID, VRAM_START, VRAM_END);

/* WRAM area */
static struct resource wram_mirror =
	MEM("echo", BUS_ID, ECHO_START, ECHO_END);

static struct resource wram_area =
	MEMX("wram", BUS_ID, WRAM_START, WRAM_END, &wram_mirror, 1);

/* HRAM area */
static struct resource hram_area =
	MEM("hram", BUS_ID, HRAM_START, HRAM_END);

/* OAM area */
static struct resource oam_area =
	MEM("oam", BUS_ID, OAM_START, OAM_END);

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
	MEM("extram", BUS_ID, EXTRAM_START, EXTRAM_END),
	MEM("lock", BUS_ID, BOOT_LOCK, BOOT_LOCK)
};

static struct controller_instance gb_mapper_instance = {
	.controller_name = "gb_mapper",
	.bus_id = BUS_ID,
	.resources = gb_mapper_resources,
	.num_resources = ARRAY_SIZE(gb_mapper_resources),
	.mach_data = &gb_mapper_mach_data
};

/* Sound controller */
static struct resource papu_resources[] = {
	MEM("mem", BUS_ID, SOUND_START, SOUND_END),
	MEM("wave", BUS_ID, WAVE_START, WAVE_END),
	CLK("clk", SOUND_CLOCK_RATE)
};

static struct controller_instance papu_instance = {
	.controller_name = "papu",
	.bus_id = BUS_ID,
	.resources = papu_resources,
	.num_resources = ARRAY_SIZE(papu_resources)
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

/* Joypad controller */
static struct resource joypad_resources[] = {
	MEM("mem", BUS_ID, JOYPAD, JOYPAD),
	IRQ("irq", JOYPAD_IRQ)
};

static struct controller_instance joypad_instance = {
	.controller_name = "gb_joypad",
	.bus_id = BUS_ID,
	.resources = joypad_resources,
	.num_resources = ARRAY_SIZE(joypad_resources)
};

/* Serial controller */
static struct resource serial_resources[] = {
	MEM("mem", BUS_ID, SERIAL_START, SERIAL_END),
	CLK("clk", SERIAL_CLOCK_RATE),
	IRQ("irq", SERIAL_IRQ)
};

static struct controller_instance serial_instance = {
	.controller_name = "gb_serial",
	.bus_id = BUS_ID,
	.resources = serial_resources,
	.num_resources = ARRAY_SIZE(serial_resources)
};

/* Timer controller */
static struct resource timer_resources[] = {
	MEM("mem", BUS_ID, TIMER_START, TIMER_END),
	CLK("div_clk", DIV_CLOCK_RATE),
	CLK("tima_clk", TIMA_CLOCK_RATE),
	IRQ("irq", TIMER_IRQ)
};

static struct controller_instance timer_instance = {
	.controller_name = "gb_timer",
	.bus_id = BUS_ID,
	.resources = timer_resources,
	.num_resources = ARRAY_SIZE(timer_resources)
};

bool gb_init(struct machine *machine)
{
	struct gb_data *gb_data;

	/* Create machine data structure */
	gb_data = calloc(1, sizeof(struct gb_data));

	/* Add 16-bit memory bus */
	gb_data->bus.id = BUS_ID;
	gb_data->bus.width = 16;
	memory_bus_add(&gb_data->bus);

	/* Add VRAM region */
	gb_data->vram_region.area = &vram_area;
	gb_data->vram_region.mops = &ram_mops;
	gb_data->vram_region.data = gb_data->vram;
	memory_region_add(&gb_data->vram_region);

	/* Add WRAM region */
	gb_data->wram_region.area = &wram_area;
	gb_data->wram_region.mops = &ram_mops;
	gb_data->wram_region.data = gb_data->wram;
	memory_region_add(&gb_data->wram_region);

	/* Add HRAM region */
	gb_data->hram_region.area = &hram_area;
	gb_data->hram_region.mops = &ram_mops;
	gb_data->hram_region.data = gb_data->hram;
	memory_region_add(&gb_data->hram_region);

	/* Add OAM region */
	gb_data->oam_region.area = &oam_area;
	gb_data->oam_region.mops = &ram_mops;
	gb_data->oam_region.data = gb_data->oam;
	memory_region_add(&gb_data->oam_region);

	/* Set GB mapper controller machine data */
	gb_mapper_mach_data.bootrom_path = bootrom_path;
	gb_mapper_mach_data.cart_path = env_get_data_path();

	/* Add controllers and CPU */
	if (!controller_add(&gb_mapper_instance) ||
		!controller_add(&papu_instance) ||
		!controller_add(&lcdc_instance) ||
		!controller_add(&joypad_instance) ||
		!controller_add(&serial_instance) ||
		!controller_add(&timer_instance) ||
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

