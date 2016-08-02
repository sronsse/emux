#include <clock.h>
#include <controller.h>
#include <memory.h>

#define COMMAND		0
#define RESPONSE	0
#define STATUS		4
#define CONTROL		4

#define FIFO_SIZE	32

#define BLOCK_Y		4
#define BLOCK_Y1	0
#define BLOCK_Y2	1
#define BLOCK_Y3	2
#define BLOCK_Y4	3
#define BLOCK_CR	4
#define BLOCK_CB	5

struct cmd_decode_macroblock {
	uint32_t num_param_words:16;
	uint32_t unused:9;
	uint32_t data_output_bit15:1;
	uint32_t data_output_signed:1;
	uint32_t data_output_depth:2;
	uint32_t cmd:3;
};

struct cmd_set_iqtab {
	uint32_t color:1;
	uint32_t unused:29;
	uint32_t cmd:3;
};

union cmd {
	uint32_t raw;
	struct {
		uint32_t bits0_15:16;
		uint32_t unused:9;
		uint32_t bits25_28:4;
		uint32_t code:3;
	};
	struct cmd_decode_macroblock decode_macroblock;
	struct cmd_set_iqtab set_iqtab;
};

struct fifo {
	uint32_t data[FIFO_SIZE];
	int pos;
	int num;
};

union stat {
	uint32_t raw;
	struct {
		uint32_t num_param_words:16;
		uint32_t current_block:3;
		uint32_t unused:4;
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
	union cmd cmd;
	struct fifo fifo_in;
	struct fifo fifo_out;
	struct region region;
	struct dma_channel dma_in_channel;
	struct dma_channel dma_out_channel;
};

static bool mdec_init(struct controller_instance *instance);
static void mdec_reset(struct controller_instance *instance);
static void mdec_deinit(struct controller_instance *instance);
static uint32_t mdec_readl(struct mdec *mdec, address_t address);
static void mdec_writel(struct mdec *mdec, uint32_t l, address_t address);
static void mdec_dma_in_writel(struct mdec *mdec, uint32_t l);
static uint32_t mdec_dma_out_readl(struct mdec *mdec);
static void mdec_process_cmd(struct mdec *mdec);

static bool fifo_empty(struct fifo *fifo);
static bool fifo_full(struct fifo *fifo);
static bool fifo_enqueue(struct fifo *fifo, uint32_t data);
static bool fifo_dequeue(struct fifo *fifo, uint32_t *data, int size);

static void cmd_no_function(struct mdec *mdec);
static void cmd_decode_macroblock(struct mdec *mdec);
static void cmd_set_iqtab(struct mdec *mdec);
static void cmd_set_scale(struct mdec *mdec);

static struct mops mdec_mops = {
	.readl = (readl_t)mdec_readl,
	.writel = (writel_t)mdec_writel
};

static struct dma_ops mdec_dma_in_ops = {
	.writel = (dma_writel_t)mdec_dma_in_writel
};

static struct dma_ops mdec_dma_out_ops = {
	.readl = (dma_readl_t)mdec_dma_out_readl
};

/* MDEC(0) - No function */
void cmd_no_function(struct mdec *mdec)
{
}

/* MDEC(1) - Decode Macroblock(s) */
void cmd_decode_macroblock(struct mdec *mdec)
{
}

/* MDEC(2) - Set Quant Table(s) */
void cmd_set_iqtab(struct mdec *mdec)
{
}

/* MDEC(3) - Set Scale Table */
void cmd_set_scale(struct mdec *mdec)
{
}

uint32_t mdec_readl(struct mdec *mdec, address_t address)
{
	uint32_t l = 0;

	/* Handle register read */
	switch (address) {
	case RESPONSE:
		/* The data is always output as a 8x8 pixel bitmap, so, when
		manually reading from this register and using colored 16x16
		pixel macroblocks, the data from four 8x8 blocks must be
		re-ordered accordingly. For monochrome 8x8 macroblocks, no
		re-ordering is needed. */
		fifo_dequeue(&mdec->fifo_out, &l, 1);

		/* Update Data-Out Request status bit (set when DMA1 enabled and
		ready to send data) */
		mdec->stat.data_out_req = mdec->ctrl.en_data_out_req;
		mdec->stat.data_out_req &= !fifo_empty(&mdec->fifo_out);
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
		/* Enqueue command/parameter */
		fifo_enqueue(&mdec->fifo_in, l);

		/* Process command */
		mdec_process_cmd(mdec);

		/* Update Data-In Request status bit (set when DMA0 enabled and
		ready to receive data) */
		mdec->stat.data_in_req = mdec->ctrl.en_data_in_req;
		mdec->stat.data_in_req &= !fifo_full(&mdec->fifo_in);

		/* Update Data-Out Request status bit (set when DMA1 enabled and
		ready to send data) */
		mdec->stat.data_out_req = mdec->ctrl.en_data_out_req;
		mdec->stat.data_out_req &= !fifo_empty(&mdec->fifo_out);
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
			mdec->fifo_in.pos = 0;
			mdec->fifo_in.num = 0;
			mdec->fifo_out.pos = 0;
			mdec->fifo_out.num = 0;
		}

		/* Update Data-In Request status bit (set when DMA0 enabled and
		ready to receive data) */
		mdec->stat.data_in_req = mdec->ctrl.en_data_in_req;
		mdec->stat.data_in_req &= !fifo_full(&mdec->fifo_in);

		/* Update Data-Out Request status bit (set when DMA1 enabled and
		ready to send data) */
		mdec->stat.data_out_req = mdec->ctrl.en_data_out_req;
		mdec->stat.data_out_req &= !fifo_empty(&mdec->fifo_out);
		break;
	}
}

void mdec_dma_in_writel(struct mdec *mdec, uint32_t l)
{
	/* Consume 1 clk/word */
	clock_consume(1);

	/* Return already if channel is not enabled */
	if (!mdec->ctrl.en_data_in_req)
		return;

	/* DMA operation is equivalent to writing to command register */
	mdec_writel(mdec, l, COMMAND);
}

uint32_t mdec_dma_out_readl(struct mdec *mdec)
{
	/* Consume 1 clk/word */
	clock_consume(1);

	/* Return already if channel is not enabled */
	if (!mdec->ctrl.en_data_out_req)
		return 0;

	/* DMA operation is equivalent to reading from response register */
	return mdec_readl(mdec, RESPONSE);
}

bool fifo_empty(struct fifo *fifo)
{
	/* Return whether FIFO is empty or not */
	return (fifo->num == 0);
}

bool fifo_full(struct fifo *fifo)
{
	/* Return whether FIFO is full or not */
	return (fifo->num == FIFO_SIZE);
}

bool fifo_enqueue(struct fifo *fifo, uint32_t data)
{
	/* Return already if FIFO is full */
	if (fifo_full(fifo))
		return false;

	/* Add command/data to FIFO and handle position overflow */
	fifo->data[fifo->pos++] = data;
	if (fifo->pos == FIFO_SIZE)
		fifo->pos = 0;

	/* Increment number of elements */
	fifo->num++;

	/* Return success */
	return true;
}

bool fifo_dequeue(struct fifo *fifo, uint32_t *data, int size)
{
	int index;
	int i;

	/* Return if FIFO does not have enough elements */
	if (fifo->num < size)
		return false;

	/* Remove command/data from FIFO */
	for (i = 0; i < size; i++) {
		index = ((fifo->pos - fifo->num) + FIFO_SIZE) % FIFO_SIZE;
		data[i] = fifo->data[index];
		fifo->num--;
	}

	/* Return success */
	return true;
}

void mdec_process_cmd(struct mdec *mdec)
{
	/* Dequeue command from FIFO if needed */
	if (!mdec->stat.cmd_busy)
		fifo_dequeue(&mdec->fifo_in, &mdec->cmd.raw, 1);

	/* Process command */
	switch (mdec->cmd.code) {
	case 0x00:
		cmd_no_function(mdec);
		break;
	case 0x01:
		cmd_decode_macroblock(mdec);
		break;
	case 0x02:
		cmd_set_iqtab(mdec);
		break;
	case 0x03:
		cmd_set_scale(mdec);
		break;
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		/* These commands act identical as MDEC(0) */
		cmd_no_function(mdec);
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

	/* Add MDECin DMA channel */
	res = resource_get("dma_in",
		RESOURCE_DMA,
		instance->resources,
		instance->num_resources);
	mdec->dma_in_channel.res = res;
	mdec->dma_in_channel.ops = &mdec_dma_in_ops;
	mdec->dma_in_channel.data = mdec;
	dma_channel_add(&mdec->dma_in_channel);

	/* Add MDECout DMA channel */
	res = resource_get("dma_out",
		RESOURCE_DMA,
		instance->resources,
		instance->num_resources);
	mdec->dma_out_channel.res = res;
	mdec->dma_out_channel.ops = &mdec_dma_out_ops;
	mdec->dma_out_channel.data = mdec;
	dma_channel_add(&mdec->dma_out_channel);

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

	/* Reset FIFOs */
	mdec->fifo_in.pos = 0;
	mdec->fifo_in.num = 0;
	mdec->fifo_out.pos = 0;
	mdec->fifo_out.num = 0;
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

