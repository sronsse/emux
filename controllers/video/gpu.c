#include <stdlib.h>
#include <string.h>
#include <clock.h>
#include <controller.h>
#include <memory.h>
#include <util.h>

#define GP0		0x00
#define GP1		0x04
#define GPUREAD		0x00
#define GPUSTAT		0x04

#define VRAM_SIZE	MB(1)
#define FIFO_SIZE	16

union cmd {
	uint32_t raw;
	struct {
		uint32_t data:24;
		uint32_t opcode:8;
	};
};

struct poly_line_data {
	int step;
	union vertex v1;
	union vertex v2;
	union color_attr color1;
	union color_attr color2;
};

struct copy_data {
	uint16_t x;
	uint16_t y;
	uint16_t min_x;
	uint16_t min_y;
	uint16_t max_x;
};

union stat {
	uint32_t raw;
	struct {
		uint32_t tex_page_x_base:4;
		uint32_t tex_page_y_base:1;
		uint32_t semi_transparency:2;
		uint32_t tex_page_colors:2;
		uint32_t dither:1;
		uint32_t drawing_allowed:1;
		uint32_t set_mask_bit:1;
		uint32_t draw_pixels:1;
		uint32_t reserved:1;
		uint32_t reverse:1;
		uint32_t tex_disable:1;
		uint32_t horizontal_res_2:1;
		uint32_t horizontal_res_1:2;
		uint32_t vertical_res:1;
		uint32_t video_mode:1;
		uint32_t color_depth:1;
		uint32_t vertical_interlace:1;
		uint32_t display_disable:1;
		uint32_t irq:1;
		uint32_t dma_data_req:1;
		uint32_t ready_recv_cmd:1;
		uint32_t ready_send_vram:1;
		uint32_t ready_recv_dma:1;
		uint32_t dma_dir:2;
		uint32_t odd:1;
	};
};

struct fifo {
	uint32_t data[FIFO_SIZE];
	int pos;
	int num;
	bool cmd_in_progress;
	uint8_t cmd_opcode;
	int cmd_half_word_count;
};

struct read_buffer {
	uint8_t cmd;
	uint32_t data;
	struct copy_data copy_data;
};

struct gpu {
	uint8_t vram[VRAM_SIZE];
	union stat stat;
	uint8_t tex_window_mask_x;
	uint8_t tex_window_mask_y;
	uint8_t tex_window_offset_x;
	uint8_t tex_window_offset_y;
	uint16_t drawing_area_x1;
	uint16_t drawing_area_y1;
	uint16_t drawing_area_x2;
	uint16_t drawing_area_y2;
	int16_t drawing_offset_x;
	int16_t drawing_offset_y;
	uint16_t display_area_src_x;
	uint16_t display_area_src_y;
	uint16_t display_area_dest_x1;
	uint16_t display_area_dest_x2;
	uint16_t display_area_dest_y1;
	uint16_t display_area_dest_y2;
	bool x_flip;
	bool y_flip;
	bool tex_disable;
	struct read_buffer read_buffer;
	struct poly_line_data poly_line_data;
	struct copy_data copy_data;
	struct fifo fifo;
	struct region region;
	struct dma_channel dma_channel;
};

static bool gpu_init(struct controller_instance *instance);
static void gpu_reset();
static void gpu_deinit(struct controller_instance *instance);
static uint32_t gpu_readl(struct gpu *gpu, address_t address);
static void gpu_writel(struct gpu *gpu, uint32_t l, address_t address);
static uint32_t gpu_dma_readl(struct gpu *gpu);
static void gpu_dma_writel(struct gpu *gpu, uint32_t l);
static void gpu_process_fifo(struct gpu *gpu);
static void gpu_gp0_cmd(struct gpu *gpu, union cmd cmd);
static void gpu_gp1_cmd(struct gpu *gpu, union cmd cmd);

static inline bool fifo_empty(struct fifo *fifo);
static inline bool fifo_full(struct fifo *fifo);
static inline uint8_t fifo_cmd(struct fifo *fifo);
static inline bool fifo_enqueue(struct fifo *fifo, uint32_t data);
static inline bool fifo_dequeue(struct fifo *fifo, uint32_t *data, int size);

static struct mops gpu_mops = {
	.readl = (readl_t)gpu_readl,
	.writel = (writel_t)gpu_writel
};

static struct dma_ops gpu_dma_ops = {
	.readl = (dma_readl_t)gpu_dma_readl,
	.writel = (dma_writel_t)gpu_dma_writel
};

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

uint8_t fifo_cmd(struct fifo *fifo)
{
	union cmd cmd;
	int index;

	/* Return command opcode in original enquued element */
	index = ((fifo->pos - fifo->num) + FIFO_SIZE) % FIFO_SIZE;
	cmd.raw = fifo->data[index];
	return cmd.opcode;
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

uint32_t gpu_readl(struct gpu *gpu, address_t address)
{
	/* Handle read */
	switch (address) {
	case GPUREAD:
		return 0;
	case GPUSTAT:
	default:
		/* Return status register */
		return gpu->stat.raw;
	}
}

void gpu_writel(struct gpu *gpu, uint32_t l, address_t address)
{
	union cmd cmd;

	/* Capture raw command */
	cmd.raw = l;

	/* Call appropriate command handler based on address */
	switch (address) {
	case GP0:
		gpu_gp0_cmd(gpu, cmd);
		break;
	case GP1:
	default:
		gpu_gp1_cmd(gpu, cmd);
		break;
	}
}

uint32_t gpu_dma_readl(struct gpu *gpu)
{
	/* Consume a single clock cycle */
	clock_consume(1);

	/* DMA operation is equivalent to accessing GPUREAD by software */
	return gpu_readl(gpu, GPUREAD);
}

void gpu_dma_writel(struct gpu *gpu, uint32_t l)
{
	/* Consume a single clock cycle */
	clock_consume(1);

	/* DMA operation is equivalent to accessing GP0 by software */
	gpu_writel(gpu, l, GP0);
}

void gpu_process_fifo(struct gpu *gpu)
{
	struct fifo *fifo = &gpu->fifo;

	/* Check if command is not being processed already */
	if (!fifo->cmd_in_progress) {
		/* Return if FIFO is empty */
		if (fifo_empty(fifo))
			return;

		/* Retrieve command opcode */
		fifo->cmd_opcode = fifo_cmd(fifo);

		/* Flag command as in progress */
		fifo->cmd_in_progress = true;

		/* Reset command-related data */
		fifo->cmd_half_word_count = 0;
		gpu->poly_line_data.step = 0;
	}

	/* Handle GP0 command */
	switch (fifo->cmd_opcode) {
		LOG_W("Unhandled GP0 opcode (%02x)!\n", fifo->cmd_opcode);
		break;
	}
}

void gpu_gp0_cmd(struct gpu *gpu, union cmd cmd)
{
	/* Handle immediate commands */
	switch (cmd.opcode) {
	default:
		LOG_W("Unhandled GP0 opcode (%02x)!\n", cmd.opcode);
		break;
	}

	/* Add command/data to FIFO */
	fifo_enqueue(&gpu->fifo, cmd.raw);

	/* Process FIFO commands */
	gpu_process_fifo(gpu);
}

void gpu_gp1_cmd(struct gpu *UNUSED(gpu), union cmd cmd)
{
	/* Execute command */
	switch (cmd.opcode) {
	default:
		LOG_W("Unhandled GP1 opcode (%02x)!\n", cmd.opcode);
		break;
	}
}

bool gpu_init(struct controller_instance *instance)
{
	struct gpu *gpu;
	struct resource *res;

	/* Allocate GPU structure */
	instance->priv_data = calloc(1, sizeof(struct gpu));
	gpu = instance->priv_data;

	/* Add GPU memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	gpu->region.area = res;
	gpu->region.mops = &gpu_mops;
	gpu->region.data = gpu;
	memory_region_add(&gpu->region);

	/* Add GPU DMA channel */
	res = resource_get("dma",
		RESOURCE_DMA,
		instance->resources,
		instance->num_resources);
	gpu->dma_channel.res = res;
	gpu->dma_channel.ops = &gpu_dma_ops;
	gpu->dma_channel.data = gpu;
	dma_channel_add(&gpu->dma_channel);

	return true;
}

void gpu_reset(struct controller_instance *instance)
{
	struct gpu *gpu = instance->priv_data;

	/* Reset private data */
	memset(gpu->vram, 0, VRAM_SIZE);
	gpu->stat.raw = 0;
	gpu->stat.reserved = 1;
	gpu->tex_window_mask_x = 0;
	gpu->tex_window_mask_y = 0;
	gpu->tex_window_offset_x = 0;
	gpu->tex_window_offset_y = 0;
	gpu->drawing_area_x1 = 0;
	gpu->drawing_area_y1 = 0;
	gpu->drawing_area_x2 = 0;
	gpu->drawing_area_y2 = 0;
	gpu->drawing_offset_x = 0;
	gpu->drawing_offset_y = 0;
	gpu->display_area_src_x = 0;
	gpu->display_area_src_y = 0;
	gpu->display_area_dest_x1 = 0;
	gpu->display_area_dest_x2 = 0;
	gpu->display_area_dest_y1 = 0;
	gpu->display_area_dest_y2 = 0;
	gpu->fifo.pos = 0;
	gpu->fifo.num = 0;
	gpu->fifo.cmd_in_progress = false;
	gpu->poly_line_data.step = 0;
	gpu->tex_disable = false;

	/* Enable clock */
	gpu->clock.enabled = true;
}

void gpu_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(gpu)
	.init = gpu_init,
	.reset = gpu_reset,
	.deinit = gpu_deinit
CONTROLLER_END

