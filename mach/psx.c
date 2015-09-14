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
#define CPU_CLOCK_RATE		33868800
#define GPU_CLOCK_RATE		53222400

/* Bus definitions */
#define CPU_BUS_ID		0

/* Memory map */
#define RAM_START		0x00000000
#define RAM_END			0x001FFFFF
#define SCRATCHPAD_START	0x1F800000
#define SCRATCHPAD_END		0x1F8003FF
#define CTRL_START		0x1F801040
#define CTRL_END		0x1F80104F
#define INT_CONTROL_START	0x1F801070
#define INT_CONTROL_END		0x1F801077
#define DMA_START		0x1F801080
#define DMA_END			0x1F8010FF
#define TIMER_START		0x1F801100
#define TIMER_END		0x1F80112F
#define CDROM_START		0x1F801800
#define CDROM_END		0x1F801803
#define GPU_START		0x1F801810
#define GPU_END			0x1F801817
#define MDEC_START		0x1F801820
#define MDEC_END		0x1F801827
#define SPU_START		0x1F801C00
#define SPU_END			0x1F801FFF
#define BIOS_START		0x1FC00000
#define BIOS_END		0x1FC7FFFF
#define CACHE_CTRL_START	0xFFFE0130
#define CACHE_CTRL_END		0xFFFE0133

/* Memory sizes */
#define RAM_SIZE		KB(2048)
#define BIOS_SIZE		KB(512)

/* DMA channels */
#define MDEC_IN_DMA_CHANNEL	0
#define MDEC_OUT_DMA_CHANNEL	1
#define GPU_DMA_CHANNEL		2
#define CDROM_DMA_CHANNEL	3
#define SPU_DMA_CHANNEL		4

/* IRQ numbers */
#define VBLANK_IRQ		0
#define GPU_IRQ			1
#define CDROM_IRQ		2
#define DMA_IRQ			3
#define TMR0_IRQ		4
#define TMR1_IRQ		5
#define TMR2_IRQ		6
#define CTRL_IRQ		7

/* Controller peripheral ports */
#define DUALSHOCK_PORT		1

struct psx_data {
	uint8_t ram[RAM_SIZE];
	uint8_t *bios;
	struct region ram_region;
	struct region bios_region;
};

static bool psx_init(struct machine *machine);
static void psx_deinit(struct machine *machine);

/* Command-line parameters */
static char *bios_path = "scph5501.bin";
PARAM(bios_path, string, "bios", "psx", "PSX BIOS path")

/* RAM area */
static struct resource ram_area =
	MEM("ram", CPU_BUS_ID, RAM_START, RAM_END);

/* BIOS area */
static struct resource bios_area =
	MEM("bios", CPU_BUS_ID, BIOS_START, BIOS_END);

/* R3051 CPU */
static struct resource cpu_resources[] = {
	CLK("clk", CPU_CLOCK_RATE),
	MEM("scratchpad", CPU_BUS_ID, SCRATCHPAD_START, SCRATCHPAD_END),
	MEM("int_control", CPU_BUS_ID, INT_CONTROL_START, INT_CONTROL_END),
	MEM("cache_control", CPU_BUS_ID, CACHE_CTRL_START, CACHE_CTRL_END)
};

static struct cpu_instance cpu_instance = {
	.cpu_name = "r3051",
	.bus_id = CPU_BUS_ID,
	.resources = cpu_resources,
	.num_resources = ARRAY_SIZE(cpu_resources)
};

/* CD-ROM */
static struct resource cdrom_resources[] = {
	MEM("mem", CPU_BUS_ID, CDROM_START, CDROM_END),
	DMA("dma", CDROM_DMA_CHANNEL),
	IRQ("irq", CDROM_IRQ),
	CLK("clk", CPU_CLOCK_RATE)
};

static struct controller_instance cdrom_instance = {
	.controller_name = "psx_cdrom",
	.resources = cdrom_resources,
	.num_resources = ARRAY_SIZE(cdrom_resources)
};

/* Controller and memory card */
static struct resource controller_resources[] = {
	MEM("mem", CPU_BUS_ID, CTRL_START, CTRL_END),
	IRQ("irq", CTRL_IRQ),
	CLK("clk", CPU_CLOCK_RATE)
};

static struct controller_instance controller_instance = {
	.controller_name = "psx_controller",
	.resources = controller_resources,
	.num_resources = ARRAY_SIZE(controller_resources)
};

/* DMA */
static struct resource dma_resources[] = {
	MEM("mem", CPU_BUS_ID, DMA_START, DMA_END),
	IRQ("irq", DMA_IRQ),
	CLK("clk", CPU_CLOCK_RATE)
};

static struct controller_instance dma_instance = {
	.controller_name = "psx_dma",
	.bus_id = CPU_BUS_ID,
	.resources = dma_resources,
	.num_resources = ARRAY_SIZE(dma_resources)
};

/* GPU */
static struct resource gpu_resources[] = {
	MEM("mem", CPU_BUS_ID, GPU_START, GPU_END),
	DMA("dma", GPU_DMA_CHANNEL),
	IRQ("vblk_irq", VBLANK_IRQ),
	IRQ("gpu_irq", GPU_IRQ),
	CLK("clk", GPU_CLOCK_RATE)
};

static struct controller_instance gpu_instance = {
	.controller_name = "gpu",
	.resources = gpu_resources,
	.num_resources = ARRAY_SIZE(gpu_resources)
};

/* MDEC */
static struct resource mdec_resources[] = {
	MEM("mem", CPU_BUS_ID, MDEC_START, MDEC_END),
	DMA("dma_in", MDEC_IN_DMA_CHANNEL),
	DMA("dma_out", MDEC_OUT_DMA_CHANNEL)
};

static struct controller_instance mdec_instance = {
	.controller_name = "mdec",
	.resources = mdec_resources,
	.num_resources = ARRAY_SIZE(mdec_resources)
};

/* SPU */
static struct resource spu_resources[] = {
	MEM("mem", CPU_BUS_ID, SPU_START, SPU_END),
	DMA("dma", SPU_DMA_CHANNEL)
};

static struct controller_instance spu_instance = {
	.controller_name = "spu",
	.resources = spu_resources,
	.num_resources = ARRAY_SIZE(spu_resources)
};

/* Timers */
static struct resource timer_resources[] = {
	MEM("mem", CPU_BUS_ID, TIMER_START, TIMER_END),
	IRQ("tmr0_irq", TMR0_IRQ),
	IRQ("tmr1_irq", TMR1_IRQ),
	IRQ("tmr2_irq", TMR2_IRQ),
	CLK("clk", CPU_CLOCK_RATE)
};

static struct controller_instance timer_instance = {
	.controller_name = "psx_timer",
	.resources = timer_resources,
	.num_resources = ARRAY_SIZE(timer_resources)
};

/* DualShock peripheral controller */
static struct resource dualshock_resource =
	PORT("port", DUALSHOCK_PORT, DUALSHOCK_PORT);

static struct controller_instance dualshock_instance = {
	.controller_name = "psx_dualshock",
	.resources = &dualshock_resource,
	.num_resources = 1,
	.mach_data = &controller_instance
};

bool psx_init(struct machine *machine)
{
	struct psx_data *data;

	/* Create machine data structure */
	data = calloc(1, sizeof(struct psx_data));
	machine->priv_data = data;

	/* Map BIOS */
	data->bios = file_map(PATH_SYSTEM,
		bios_path,
		0,
		BIOS_SIZE);
	if (!data->bios) {
		LOG_E("Could not map BIOS!\n");
		free(data);
		return false;
	}

	/* Initialize RAM region */
	data->ram_region.area = &ram_area;
	data->ram_region.mops = &ram_mops;
	data->ram_region.data = data->ram;
	memory_region_add(&data->ram_region);

	/* Initialize BIOS region */
	data->bios_region.area = &bios_area;
	data->bios_region.mops = &rom_mops;
	data->bios_region.data = data->bios;
	memory_region_add(&data->bios_region);

	/* Add controllers and CPU */
	if (!cpu_add(&cpu_instance) ||
		!controller_add(&cdrom_instance) ||
		!controller_add(&controller_instance) ||
		!controller_add(&dma_instance) ||
		!controller_add(&gpu_instance) ||
		!controller_add(&mdec_instance) ||
		!controller_add(&spu_instance) ||
		!controller_add(&timer_instance) ||
		!controller_add(&dualshock_instance)) {
		file_unmap(data->bios, BIOS_SIZE);
		free(data);
		return false;
	}

	return true;
}

void psx_deinit(struct machine *machine)
{
	struct psx_data *data = machine->priv_data;

	file_unmap(data->bios, BIOS_SIZE);
	free(machine->priv_data);
}

MACHINE_START(psx, "Sony PlayStation")
	.init = psx_init,
	.deinit = psx_deinit
MACHINE_END

