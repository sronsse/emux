#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmdline.h>
#include <cpu.h>
#include <machine.h>
#include <memory.h>
#include <util.h>

#define RAM_START	0x0000
#define RAM_END		0x0FFF

#define ROM_ADDRESS	0x0200

static bool chip8_init();
static void chip8_deinit();
static void chip8_print_usage();
static uint8_t ram_readb(region_data_t *data, uint16_t address);
static void ram_writeb(region_data_t *data, uint8_t b, uint16_t address);

static struct cpu_instance chip8_cpu_instance = {
	.cpu_name = "chip8"
};

static struct resource ram_area = {
	.name = "ram",
	.start = RAM_START,
	.end = RAM_END,
	.type = RESOURCE_MEM
};

static struct mops ram_mops = {
	.readb = ram_readb,
	.writeb = ram_writeb
};

static struct region ram_region = {
	.area = &ram_area,
	.mops = &ram_mops
};

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

uint8_t ram_readb(region_data_t *data, uint16_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	return *(uint8_t *)mem;
}

void ram_writeb(region_data_t *data, uint8_t b, uint16_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	*mem = b;
}

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
	uint8_t *ram;

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
	max_rom_size = RAM_END - ROM_ADDRESS + 1;
	size = (size < max_rom_size) ? size : max_rom_size;

	/* Create and add RAM region */
	ram = malloc(RAM_END - RAM_START + 1);
	ram_region.data = ram;
	memory_region_add(&ram_region);

	/* Copy character memory to beginning of RAM */
	memcpy(ram, char_mem, ARRAY_SIZE(char_mem));

	/* Copy ROM contents to RAM (starting at ROM address) */
	if (fread(&ram[ROM_ADDRESS], 1, size, f) != size) {
		free(ram);
		fclose(f);
		fprintf(stderr, "Could not read ROM from \"%s\"!\n", rom_path);
		return false;
	}
	fclose(f);

	cpu_add(&chip8_cpu_instance);

	return true;
}

void chip8_deinit()
{
	free(ram_region.data);
}

MACHINE_START(chip8, "CHIP-8")
	.init = chip8_init,
	.deinit = chip8_deinit
MACHINE_END

