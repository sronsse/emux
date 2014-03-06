#include <stdlib.h>
#include <string.h>
#include <cpu.h>
#include <env.h>
#include <file.h>
#include <log.h>
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

struct chip8_data {
	uint8_t ram[RAM_SIZE];
};

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

bool chip8_init(struct machine *machine)
{
	struct chip8_data *chip8_data;
	file_handle_t f;
	char *rom_path;
	unsigned int size;
	unsigned int max_rom_size;

	/* Create machine data structure */
	chip8_data = malloc(sizeof(struct chip8_data));

	/* Open ROM file */
	rom_path = env_get_data_path();
	f = file_open(PATH_DATA, rom_path, "rb");
	if (!f) {
		LOG_E("Could not open ROM from \"%s\"!\n", rom_path);
		return false;
	}

	/* Make sure ROM file is not too large */
	size = file_get_size(f);
	max_rom_size = RAM_SIZE - ROM_ADDRESS;
	size = (size < max_rom_size) ? size : max_rom_size;

	/* Add 16-bit memory bus */
	memory_bus_add(16);

	/* Add RAM region */
	memory_region_add(&ram_area, &ram_mops, chip8_data->ram);

	/* Copy character memory to beginning of RAM */
	memcpy(chip8_data->ram, char_mem, ARRAY_SIZE(char_mem));

	/* Copy ROM contents to RAM (starting at ROM address) */
	if (!file_read(f, &chip8_data->ram[ROM_ADDRESS], 0, size)) {
		free(chip8_data);
		file_close(f);
		LOG_E("Could not read ROM from \"%s\"!\n", rom_path);
		return false;
	}
	file_close(f);

	if (!cpu_add(&chip8_cpu_instance)) {
		free(chip8_data);
		return false;
	}

	/* Save machine data structure */
	machine->priv_data = chip8_data;

	return true;
}

void chip8_deinit(struct machine *machine)
{
	free(machine->priv_data);
}

MACHINE_START(chip8, "CHIP-8")
	.init = chip8_init,
	.deinit = chip8_deinit
MACHINE_END

