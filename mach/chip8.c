#include <stdlib.h>
#include <string.h>
#include <cpu.h>
#include <env.h>
#include <file.h>
#include <log.h>
#include <machine.h>
#include <memory.h>
#include <util.h>

/* Bus definitions */
#define CPU_BUS_ID	0

/* Memory sizes */
#define RAM_SIZE	4096

/* Memory map */
#define RAM_START	0x0000
#define RAM_END		0x0FFF
#define ROM_ADDRESS	0x0200

static bool chip8_init(struct machine *machine);
static void chip8_reset(struct machine *machine);
static void chip8_deinit(struct machine *machine);

struct chip8_data {
	file_handle_t rom_file;
	uint8_t ram[RAM_SIZE];
	struct bus bus;
	struct region ram_region;
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
	char *rom_path;

	/* Create machine data structure */
	chip8_data = malloc(sizeof(struct chip8_data));

	/* Open ROM file */
	rom_path = env_get_data_path();
	chip8_data->rom_file = file_open(PATH_DATA, rom_path, "rb");
	if (!chip8_data->rom_file) {
		free(chip8_data);
		LOG_E("Could not open ROM from \"%s\"!\n", rom_path);
		return false;
	}

	/* Add 16-bit memory bus */
	chip8_data->bus.id = CPU_BUS_ID;
	chip8_data->bus.width = 16;
	memory_bus_add(&chip8_data->bus);

	/* Add RAM region */
	chip8_data->ram_region.area = &ram_area;
	chip8_data->ram_region.mops = &ram_mops;
	chip8_data->ram_region.data = chip8_data->ram;
	memory_region_add(&chip8_data->ram_region);

	/* Add CPU */
	if (!cpu_add(&chip8_cpu_instance)) {
		file_close(chip8_data->rom_file);
		free(chip8_data);
		return false;
	}

	/* Save machine data structure */
	machine->priv_data = chip8_data;

	return true;
}

void chip8_reset(struct machine *machine)
{
	struct chip8_data *data = machine->priv_data;
	unsigned int size;
	unsigned int max_rom_size;

	/* Copy character memory to beginning of RAM */
	memcpy(data->ram, char_mem, ARRAY_SIZE(char_mem));

	/* Make sure ROM file is not too large */
	size = file_get_size(data->rom_file);
	max_rom_size = RAM_SIZE - ROM_ADDRESS;
	size = (size < max_rom_size) ? size : max_rom_size;

	/* Copy ROM contents to RAM (starting at ROM address) */
	if (!file_read(data->rom_file, &data->ram[ROM_ADDRESS], 0, size))
		LOG_E("Could not read ROM!\n");
}

void chip8_deinit(struct machine *machine)
{
	struct chip8_data *data = machine->priv_data;
	file_close(data->rom_file);
	free(data);
}

MACHINE_START(chip8, "CHIP-8")
	.init = chip8_init,
	.reset = chip8_reset,
	.deinit = chip8_deinit
MACHINE_END

