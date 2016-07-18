#include <stdlib.h>
#include <controller.h>
#include <cpu.h>
#include <file.h>
#include <log.h>
#include <machine.h>
#include <memory.h>
#include <port.h>
#include <resource.h>
#include <util.h>

/* Clock frequencies */
#define MASTER_CLOCK_RATE	53693100.0f
#define AUDIO_CLOCK_RATE	(MASTER_CLOCK_RATE / 15)
#define CPU_CLOCK_RATE		(MASTER_CLOCK_RATE / 15)
#define VDP_CLOCK_RATE		(MASTER_CLOCK_RATE / 5)

/* Interrupt definitions */
#define VDP_IRQ			0
#define PAUSE_IRQ		1

/* Bus definitions */
#define CPU_BUS_ID		0
#define VDP_BUS_ID		1

/* Memory sizes */
#define RAM_SIZE		0x2000

/* Memory map */
#define MAPPER_START		0x0000
#define MAPPER_END		0xBFFF
#define RAM_START		0xC000
#define RAM_END			0xDFFF
#define ECHO_START		0xE000
#define ECHO_END		0xFFFF

/* Port map */
#define IO_CTL_PORT_START	0x01
#define IO_CTL_PORT_END		0x3F
#define MAPPER_PORT		0x3E
#define AUDIO_PORT		0x7F
#define AUDIO_PORT_MIRROR_START	0x40
#define AUDIO_PORT_MIRROR_END	0x7E
#define VDP_PORT_START		0xBE
#define VDP_PORT_END		0xBF
#define VDP_PORT_MIRROR_START	0x80
#define VDP_PORT_MIRROR_END	0xBD
#define IO_DATA_PORT_START	0xC0
#define IO_DATA_PORT_END	0xFF
#define VDP_SCANLINE_PORT_START	0x7E
#define VDP_SCANLINE_PORT_END	0x7F

struct sms_data {
	uint8_t ram[RAM_SIZE];
	struct region ram_region;
};

static bool sms_init(struct machine *machine);
static void sms_deinit(struct machine *machine);

/* Z80A CPU */
static struct resource cpu_resources[] = {
	CLK("clk", CPU_CLOCK_RATE)
};

static struct cpu_instance cpu_instance = {
	.cpu_name = "z80",
	.bus_id = CPU_BUS_ID,
	.resources = cpu_resources,
	.num_resources = ARRAY_SIZE(cpu_resources)
};

/* Audio controller */
static struct resource sn76489_mirror =
	PORT("port_mirror", AUDIO_PORT_MIRROR_START, AUDIO_PORT_MIRROR_END);

static struct resource sn76489_resources[] = {
	PORTX("port", AUDIO_PORT, AUDIO_PORT, &sn76489_mirror, 1),
	CLK("clk", AUDIO_CLOCK_RATE)
};

static struct controller_instance sn76489_instance = {
	.controller_name = "sn76489",
	.resources = sn76489_resources,
	.num_resources = ARRAY_SIZE(sn76489_resources)
};

/* SMS mapper controller */
static struct resource sms_mapper_resources[] = {
	MEM("mapper", CPU_BUS_ID, MAPPER_START, MAPPER_END),
	PORT("port", MAPPER_PORT, MAPPER_PORT)
};

static struct controller_instance sms_mapper_instance = {
	.controller_name = "sms_mapper",
	.bus_id = CPU_BUS_ID,
	.resources = sms_mapper_resources,
	.num_resources = ARRAY_SIZE(sms_mapper_resources)
};

/* Input controller */
static struct resource input_resources[] = {
	PORT("ctl", IO_CTL_PORT_START, IO_CTL_PORT_END),
	PORT("io", IO_DATA_PORT_START, IO_DATA_PORT_END),
	IRQ("pause_irq", PAUSE_IRQ)
};

static struct controller_instance input_instance = {
	.controller_name = "sms_controller",
	.resources = input_resources,
	.num_resources = ARRAY_SIZE(input_resources)
};

/* VDP controller */
static struct resource vdp_mirror =
	PORT("port_mirror", VDP_PORT_MIRROR_START, VDP_PORT_MIRROR_END);

static struct resource vdp_resources[] = {
	PORTX("port", VDP_PORT_START, VDP_PORT_END, &vdp_mirror, 1),
	PORT("scanline", VDP_SCANLINE_PORT_START, VDP_SCANLINE_PORT_END),
	IRQ("irq", VDP_IRQ),
	CLK("clk", VDP_CLOCK_RATE)
};

static struct controller_instance vdp_instance = {
	.controller_name = "vdp",
	.bus_id = VDP_BUS_ID,
	.resources = vdp_resources,
	.num_resources = ARRAY_SIZE(vdp_resources)
};

/* RAM area */
static struct resource ram_mirror =
	MEM("echo", CPU_BUS_ID, ECHO_START, ECHO_END);

static struct resource ram_area =
	MEMX("ram", CPU_BUS_ID, RAM_START, RAM_END, &ram_mirror, 1);

bool sms_init(struct machine *machine)
{
	struct sms_data *data;

	/* Create machine data structure */
	data = calloc(1, sizeof(struct sms_data));
	machine->priv_data = data;

	/* Add RAM region */
	data->ram_region.area = &ram_area;
	data->ram_region.mops = &ram_mops;
	data->ram_region.data = data->ram;
	memory_region_add(&data->ram_region);

	/* Add controllers and CPU */
	if (!controller_add(&sn76489_instance) ||
		!controller_add(&vdp_instance) ||
		!controller_add(&input_instance) ||
		!controller_add(&sms_mapper_instance) ||
		!cpu_add(&cpu_instance)) {
		free(data);
		return false;
	}

	return true;
}

void sms_deinit(struct machine *machine)
{
	free(machine->priv_data);
}

MACHINE_START(sms, "Sega Master System")
	.init = sms_init,
	.deinit = sms_deinit
MACHINE_END

