#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmdline.h>
#include <cpu.h>
#include <machine.h>
#include <memory.h>
#include <util.h>

#define CPU_BUS_ID	0
#define RAM_SIZE	4096
#define RAM_START	0x0000
#define RAM_END		0x0FFF
#define ROM_ADDRESS	0x0200

static bool chip8_init();
static void chip8_deinit();
static void chip8_print_usage();

static uint8_t ram[RAM_SIZE];

static struct cpu_instance chip8_cpu_instance = {
	.cpu_name = "chip8",
	.bus_id = CPU_BUS_ID
};

static struct resource ram_area =
	MEM("mem", CPU_BUS_ID, RAM_START, RAM_END);

static uint8_t char_mem[] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0,
	0x20, 0x60, 0x20, 0x20, 0x70,
	0xF0, 0x10, 0xF0, 0x80, 0xF0,
	0xF0, 0x10, 0xF0, 0x10, 0xF0,
	0x90, 0x90, 0xF0, 0x10, 0x10,
	0xF0, 0x80, 0xF0, 0x10, 0xF0,
	0xF0, 0x80, 0xF0, 0x90, 0xF0,
	0xF0, 0x10, 0x20, 0x40, 0x40,
	0xF0, 0x90, 0xF0, 0x90, 0xF0,
	0xF0, 0x90, 0xF0, 0x10, 0xF0,
	0xF0, 0x90, 0xF0, 0x90, 0x90,
	0xE0, 0x90, 0xE0, 0x90, 0xE0,
	0xF0, 0x80, 0x80, 0x80, 0xF0,
	0xE0, 0x90, 0x90, 0x90, 0xE0,
	0xF0, 0x80, 0xF0, 0x80, 0xF0,
	0xF0, 0x80, 0xF0, 0x80, 0x80
};

void chip8_print_usage()
{
	fprintf(stderr, "Valid chip8 options:\n");
	fprintf(stderr, "  --rom    ROM path\n");
}

bool chip8_init()
{
	char *rom_path;
	FILE *f;
	unsigned int size;
	unsigned int max_rom_size;

	/* Get ROM option */
	if (!cmdline_parse_string("rom", &rom_path)) {
		fprintf(stderr, "Please provide a rom option!\n");
		chip8_print_usage();
		return false;
	}

	/* Open ROM file */
	f = fopen(rom_path, "rb");
	if (!f) {
		fprintf(stderr, "Could not open ROM from \"%s\"!\n", rom_path);
		return false;
	}

	/* Get ROM file size */
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	/* Make sure ROM file is not too large */
	max_rom_size = RAM_SIZE - ROM_ADDRESS;
	size = (size < max_rom_size) ? size : max_rom_size;

	/* Create and add RAM region */
	memory_region_add(&ram_area, &ram_mops, ram);

	/* Copy character memory to beginning of RAM */
	memcpy(ram, char_mem, ARRAY_SIZE(char_mem));

	/* Copy ROM contents to RAM (starting at ROM address) */
	if (fread(&ram[ROM_ADDRESS], 1, size, f) != size) {
		fclose(f);
		fprintf(stderr, "Could not read ROM from \"%s\"!\n", rom_path);
		return false;
	}
	fclose(f);

	if (!cpu_add(&chip8_cpu_instance))
		return false;

	return true;
}

void chip8_deinit()
{
}

MACHINE_START(chip8, "CHIP-8")
	.init = chip8_init,
	.deinit = chip8_deinit
MACHINE_END

