#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmdline.h>
#include <controller.h>
#include <cpu.h>
#include <machine.h>
#include <memory.h>
#include <util.h>
#include <controllers/mapper/gb_mapper.h>

#define GB_CLOCK_RATE	4194304

#define VRAM_SIZE	KB(8)
#define HRAM_SIZE	127

/* Memory map */
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
	bool bootrom_locked;
	uint8_t *bootrom;
	uint16_t bootrom_size;
	uint8_t *rom0;
	uint16_t rom0_size;
	uint8_t vram[VRAM_SIZE];
	uint8_t hram[HRAM_SIZE];
};

static bool gb_init();
static void gb_deinit();
static void gb_print_usage();
static bool gb_map_bootrom(struct gb_data *gb_data, char *path);
static void gb_unmap_bootrom(struct gb_data *gb_data);
static bool gb_map_rom0(struct gb_data *gb_data, char *path);
static void gb_unmap_rom0(struct gb_data *gb_data);

/* Boot ROM area (end address is filled at run-time) */
static struct resource bootrom_area = MEM("bootrom", BUS_ID, ROM0_START, 0);

/* ROM0 area (start addres is filled at run-time) */
static struct resource rom0_area = MEM("rom0", BUS_ID, 0, ROM0_END);

/* VRAM area */
static struct resource vram_area = MEM("vram", BUS_ID, VRAM_START, VRAM_END);

/* HRAM area */
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
	MEM("rom1", BUS_ID, ROM1_START, ROM1_END)
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

bool gb_map_bootrom(struct gb_data *gb_data, char *path)
{
	FILE *f;
	uint16_t end_addr;

	/* Open boot ROM file */
	f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "Could not open boot ROM from \"%s\"!\n", path);
		return false;
	}

	/* Compute boot ROM size */
	fseek(f, 0, SEEK_END);
	gb_data->bootrom_size = ftell(f);
	fseek(f, 0, SEEK_SET);
	fclose(f);

	/* Map boot ROM */
	gb_data->bootrom = memory_map_file(path, 0, gb_data->bootrom_size);

	/* Specify boot ROM area end address */
	end_addr = bootrom_area.data.mem.start + gb_data->bootrom_size - 1;
	bootrom_area.data.mem.end = end_addr;

	/* Add boot ROM memory region */
	memory_region_add(&bootrom_area, &rom_mops, gb_data->bootrom);

	return true;
}

void gb_unmap_bootrom(struct gb_data *gb_data)
{
	/* Unmap boot ROM */
	memory_unmap_file(gb_data->bootrom, gb_data->bootrom_size);
}

bool gb_map_rom0(struct gb_data *gb_data, char *path)
{
	uint16_t start_addr;

	/* Initialize start address to map based on boot ROM locked state */
	start_addr = ROM0_START;
	if (!gb_data->bootrom_locked)
		start_addr += gb_data->bootrom_size;

	/* Specify ROM0 area start address */
	rom0_area.data.mem.start = start_addr;

	/* Save mapped size */
	gb_data->rom0_size = ROM0_END - start_addr + 1;

	/* Map ROM0 */
	gb_data->rom0 = memory_map_file(path, start_addr, gb_data->rom0_size);
	if (!gb_data->rom0) {
		fprintf(stderr, "Could not map cart from \"%s\"!\n", path);
		return false;
	}

	/* Add ROM0 memory region */
	memory_region_add(&rom0_area, &rom_mops, gb_data->rom0);
	return true;
}

void gb_unmap_rom0(struct gb_data *gb_data)
{
	/* Unmap ROM0 */
	memory_unmap_file(gb_data->rom0, gb_data->rom0_size);
}

void gb_print_usage()
{
	fprintf(stderr, "Valid gb options:\n");
	fprintf(stderr, "  --bootrom    GameBoy boot ROM path\n");
	fprintf(stderr, "  --cart       Game cart path\n");
}

bool gb_init(struct machine *machine)
{
	struct gb_data *gb_data;
	char *bootrom_path;
	char *cart_path;

	/* Create machine data structure and initialize it */
	gb_data = malloc(sizeof(struct gb_data));
	gb_data->bootrom_locked = false;

	/* Get bootrom option */
	if (!cmdline_parse_string("bootrom", &bootrom_path)) {
		free(gb_data);
		fprintf(stderr, "Please provide a bootrom option!\n");
		gb_print_usage();
		return false;
	}

	/* Get cart option */
	if (!cmdline_parse_string("cart", &cart_path)) {
		free(gb_data);
		fprintf(stderr, "Please provide a cart option!\n");
		gb_print_usage();
		return false;
	}

	/* Add 16-bit memory bus */
	memory_bus_add(16);

	/* Map provided boot ROM */
	if (!gb_map_bootrom(gb_data, bootrom_path)) {
		free(gb_data);
		fprintf(stderr, "Could not map boot ROM!\n");
		return false;
	}

	/* Map ROM0 using provided cart */
	if (!gb_map_rom0(gb_data, cart_path)) {
		gb_unmap_bootrom(gb_data);
		free(gb_data);
		fprintf(stderr, "Could not map ROM0!\n");
		return false;
	}

	/* Add memory regions */
	memory_region_add(&vram_area, &ram_mops, gb_data->vram);
	memory_region_add(&hram_area, &ram_mops, gb_data->hram);

	/* Set GB mapper controller machine data */
	gb_mapper_mach_data.path = cart_path;

	/* Add controllers and CPU */
	if (!controller_add(&gb_mapper_instance) ||
		!controller_add(&lcdc_instance) ||
		!cpu_add(&cpu_instance)) {
		gb_unmap_rom0(gb_data);
		gb_unmap_bootrom(gb_data);
		free(gb_data);
		return false;
	}

	/* Save machine data structure */
	machine->priv_data = gb_data;

	return true;
}

void gb_deinit(struct machine *machine)
{
	struct gb_data *gb_data = machine->priv_data;
	gb_unmap_rom0(gb_data);
	gb_unmap_bootrom(gb_data);
	free(gb_data);
}

MACHINE_START(gb, "Nintendo Game Boy")
	.init = gb_init,
	.deinit = gb_deinit
MACHINE_END

