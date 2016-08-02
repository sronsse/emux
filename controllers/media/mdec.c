#include <clock.h>
#include <controller.h>
#include <memory.h>

#define COMMAND		0
#define RESPONSE	0
#define STATUS		4
#define CONTROL		4

#define BLOCK_Y		4
#define BLOCK_Y1	0
#define BLOCK_Y2	1
#define BLOCK_Y3	2
#define BLOCK_Y4	3
#define BLOCK_CR	4
#define BLOCK_CB	5

union stat {
	uint32_t raw;
	struct {
		uint32_t num_param_words:16;
		uint32_t current_block:3;
		uint32_t unused: 4;
		uint32_t data_output_bit15:1;
		uint32_t data_output_signed:1;
		uint32_t data_output_depth:2;
		uint32_t data_out_req:1;
		uint32_t data_in_req:1;
		uint32_t cmd_busy:1;
		uint32_t data_in_fifo_full:1;
		uint32_t data_out_fifo_empty:1;
	};
	struct {
		uint32_t bits0_15:16;
		uint32_t unused1:7;
		uint32_t bits23_26:4;
		uint32_t unused2:5;
	};
};

union ctrl {
	uint32_t raw;
	struct {
		uint32_t unknown:29;
		uint32_t en_data_out_req:1;
		uint32_t en_data_in_req:1;
		uint32_t reset:1;
	};
};

struct mdec {
	union stat stat;
	union ctrl ctrl;
	struct region region;
};

static bool mdec_init(struct controller_instance *instance);
static void mdec_reset(struct controller_instance *instance);
static void mdec_deinit(struct controller_instance *instance);
static uint32_t mdec_readl(struct mdec *mdec, address_t address);
static void mdec_writel(struct mdec *mdec, uint32_t l, address_t address);

static struct mops mdec_mops = {
	.readl = (readl_t)mdec_readl,
	.writel = (writel_t)mdec_writel
};

uint32_t mdec_readl(struct mdec *mdec, address_t address)
{
	uint32_t l = 0;

	/* Handle register read */
	switch (address) {
	case RESPONSE:
		/* Read response */
		l = 0;
		break;
	case STATUS:
		/* Read status register */
		l = mdec->stat.raw;
		break;
	}

	/* Return register contents */
	return l;
}

void mdec_writel(struct mdec *mdec, uint32_t l, address_t address)
{
	/* Handle register write */
	switch (address) {
	case COMMAND:
		break;
	case CONTROL:
		/* Write control register */
		mdec->ctrl.raw = l;

		/* Handle reset request */
		if (mdec->ctrl.reset) {
			mdec->stat.raw = 0;
			mdec->stat.current_block = BLOCK_Y;
			mdec->stat.data_out_fifo_empty = 1;
			mdec->ctrl.raw = 0;
		}
		break;
	}
}

bool mdec_init(struct controller_instance *instance)
{
	struct mdec *mdec;
	struct resource *res;

	/* Allocate MDEC structure */
	instance->priv_data = calloc(1, sizeof(struct mdec));
	mdec = instance->priv_data;

	/* Add MDEC memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	mdec->region.area = res;
	mdec->region.mops = &mdec_mops;
	mdec->region.data = mdec;
	memory_region_add(&mdec->region);

	return true;
}

void mdec_reset(struct controller_instance *instance)
{
	struct mdec *mdec = instance->priv_data;

	/* Reset registers */
	mdec->stat.raw = 0;
	mdec->stat.current_block = BLOCK_Y;
	mdec->stat.data_out_fifo_empty = 1;
	mdec->ctrl.raw = 0;
}

void mdec_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(mdec)
	.init = mdec_init,
	.reset = mdec_reset,
	.deinit = mdec_deinit
CONTROLLER_END

