#include <stdlib.h>
#include <cmdline.h>
#include <controller.h>
#include <cpu.h>
#include <file.h>
#include <log.h>
#include <machine.h>
#include <memory.h>
#include <resource.h>
#include <util.h>

/* Clock frequencies */
#define SMS_CLOCK_RATE	3579540

/* Bus definitions */
#define BUS_ID		0

/* Memory sizes */
#define RAM_SIZE	0x2000

/* Memory map */
#define BIOS_START		0x0000
#define BIOS_END		0xBFFF
#define RAM_START		0xC000
#define RAM_END			0xDFFF
#define ECHO_START		0xE000
#define ECHO_END		0xFFF7

/* Port map */
#define VDP_PORT_START		0xBE
#define VDP_PORT_END		0xBF
#define VDP_PORT_MIRROR_START	0x80
#define VDP_PORT_MIRROR_END	0xBD

struct sms_data {
	int bios_size;
	uint8_t *bios;
	uint8_t ram[RAM_SIZE];
	struct bus bus;
	struct region bios_region;
	struct region ram_region;
};

static bool sms_init(struct machine *machine);
static void sms_deinit(struct machine *machine);
static bool sms_load_bios(struct sms_data *data);

/* Command-line parameters */
static char *bios_path = "bios.sms";
PARAM(bios_path, string, "bios", "sms", "SMS BIOS path")

/* Z80A CPU */
static struct resource cpu_resources[] = {
	CLK("clk", SMS_CLOCK_RATE)
};

static struct cpu_instance cpu_instance = {
	.cpu_name = "z80",
	.bus_id = BUS_ID,
	.resources = cpu_resources,
	.num_resources = ARRAY_SIZE(cpu_resources)
};

/* VDP controller */
static struct resource vdp_mirror =
	PORT("port_mirror", VDP_PORT_MIRROR_START, VDP_PORT_MIRROR_END);

static struct resource vdp_port =
	PORTX("port", VDP_PORT_START, VDP_PORT_END, &vdp_mirror, 1);

static struct controller_instance vdp_instance = {
	.controller_name = "vdp",
	.resources = &vdp_port,
	.num_resources = 1
};

/* BIOS area */
static struct resource bios_area =
	MEM("bios", BUS_ID, BIOS_START, BIOS_END);

/* RAM area */
static struct resource ram_mirror = MEM("echo", BUS_ID, ECHO_START, ECHO_END);
static struct resource ram_area =
	MEMX("ram", BUS_ID, RAM_START, RAM_END, &ram_mirror, 1);

bool sms_load_bios(struct sms_data *data)
{
	file_handle_t f;

	/* Open BIOS file, get its size, and close it */
	f = file_open(PATH_SYSTEM, bios_path, "rb");
	if (!f) {
		LOG_E("Could not open BIOS!\n");
		return false;
	}
	data->bios_size = file_get_size(f);
	file_close(f);

	/* Map BIOS */
	data->bios = file_map(PATH_SYSTEM,
		bios_path,
		0,
		data->bios_size);
	if (!data->bios) {
		LOG_E("Could not map BIOS!\n");
		return false;
	}

	/* Add BIOS region */
	data->bios_region.area = &bios_area;
	data->bios_region.mops = &rom_mops;
	data->bios_region.data = data->bios;
	memory_region_add(&data->bios_region);
	return true;
}

bool sms_init(struct machine *machine)
{
	struct sms_data *data;

	/* Create machine data structure */
	data = malloc(sizeof(struct sms_data));
	machine->priv_data = data;

	/* Add 16-bit memory bus */
	data->bus.id = BUS_ID;
	data->bus.width = 16;
	memory_bus_add(&data->bus);

	/* Load BIOS */
	if (!sms_load_bios(data)) {
		free(data);
		return false;
	}

	/* Add RAM region */
	data->ram_region.area = &ram_area;
	data->ram_region.mops = &ram_mops;
	data->ram_region.data = data->ram;
	memory_region_add(&data->ram_region);

	/* Add controllers and CPU */
	if (!controller_add(&vdp_instance) || !cpu_add(&cpu_instance)) {
		file_unmap(data->bios, data->bios_size);
		free(data);
		return false;
	}

	return true;
}

void sms_deinit(struct machine *machine)
{
	struct sms_data *data = machine->priv_data;
	file_unmap(data->bios, data->bios_size);
}

MACHINE_START(sms, "Sega Master System")
	.init = sms_init,
	.deinit = sms_deinit
MACHINE_END

