#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <bitops.h>
#include <clock.h>
#include <controller.h>
#include <cpu.h>
#include <log.h>
#include <memory.h>
#include <port.h>
#include <util.h>
#include <video.h>

/* Macros */
#define RED(v)				((v & 0x03) << 6)
#define GREEN(v)			(((v >> 2) & 0x03) << 6)
#define BLUE(v)				(((v >> 4) & 0x03) << 6)

/* VDP registers */
#define NUM_REGS			16
#define MODE_CTRL_1			0x00
#define MODE_CTRL_2			0x01
#define NAME_TABLE_BASE			0x02
#define COLOR_TABLE_BASE		0x03
#define PATTERN_GEN_TABLE_BASE		0x04
#define SPRITE_ATTR_TABLE_BASE		0x05
#define SPRITE_PATTERN_GEN_TABLE_BASE	0x06
#define OVERSCAN_COLOR			0x07
#define BG_X_SCROLL			0x08
#define BG_Y_SCROLL			0x09
#define LINE_COUNTER			0x0A

/* VDP ports */
#define DATA_PORT			0
#define CTRL_PORT			1

/* VDP constant parameters */
#define SCREEN_WIDTH			256
#define SCREEN_HEIGHT			192
#define SCREEN_REFRESH_RATE		60
#define NUM_COLUMNS			342
#define NUM_ROWS			262
#define MAX_BG_Y			224
#define VRAM_SIZE			16384
#define CRAM_SIZE			32
#define TILE_WIDTH			8
#define TILE_HEIGHT			8
#define TILE_SIZE			32
#define NUM_BIT_PLANES			4
#define SPRITE_PALETTE_OFFSET		16
#define HORI_SCROLL_LOCK_HEIGHT		16

struct mode_ctrl_1 {
	uint8_t synch_enable:1;
	uint8_t extra_height_enable:1;
	uint8_t mode_4_enable:1;
	uint8_t shift_sprites_left_8_pixels:1;
	uint8_t enable_line_interrupts:1;
	uint8_t mask_col_0:1;
	uint8_t hori_scroll_lock:1;
	uint8_t vert_scroll_lock:1;
};

struct mode_ctrl_2 {
	uint8_t double_sprites:1;
	uint8_t large_sprites:1;
	uint8_t reserved:1;
	uint8_t mode_224_lines:1;
	uint8_t mode_240_lines:1;
	uint8_t enable_frame_interrupts:1;
	uint8_t enable_display:1;
	uint8_t reserved2:1;
};

struct name_table_base_addr {
	uint8_t bit0:1;
	uint8_t addr:3;
	uint8_t reserved2:4;
};

struct sprite_attr_table_base_addr {
	uint8_t reserved:1;
	uint8_t addr:6;
	uint8_t reserved2:1;
};

struct sprite_patt_gen_base_addr {
	uint8_t reserved:2;
	uint8_t sel:1;
	uint8_t reserved2:5;
};

struct overscan_color {
	uint8_t color:4;
	uint8_t reserved:4;
};

union status {
	uint8_t raw;
	struct {
		uint8_t reserved:5;
		uint8_t sprite_collision:1;
		uint8_t sprite_overflow:1;
		uint8_t frame_interrupt_pending:1;
	};
};

union vdp_addr {
	uint16_t raw;
	struct {
		uint16_t word:1;
		uint16_t column:5;
		uint16_t row:5;
		uint16_t sel:3;
		uint16_t unused:2;
	};
};

union bg_tile {
	struct {
		uint16_t low:8;
		uint16_t high:8;
	};
	struct {
		uint16_t pattern_index:9;
		uint16_t h_flip:1;
		uint16_t v_flip:1;
		uint16_t palette_sel:1;
		uint16_t priority:1;
		uint16_t reserved:3;
	};
};

union cmd_word {
	uint16_t raw;
	struct {
		uint16_t address:14;
		uint16_t code:2;
	};
	struct {
		uint16_t data:8;
		uint16_t reg:4;
		uint16_t reserved:2;
		uint16_t code2:2;
	};
};

union vdp_regs {
	uint8_t raw[NUM_REGS];
	struct {
		struct mode_ctrl_1 mode_ctrl_1;
		struct mode_ctrl_2 mode_ctrl_2;
		struct name_table_base_addr name_table_base_addr;
		uint8_t color_table_base_addr;
		uint8_t bg_patt_gen_base_addr;
		struct sprite_attr_table_base_addr sprite_attr_table_base_addr;
		struct sprite_patt_gen_base_addr sprite_patt_gen_base_addr;
		struct overscan_color overscan_color;
		uint8_t bg_x_scroll;
		uint8_t bg_y_scroll;
		uint8_t line_counter;
		uint8_t reserved[5];
	};
};

struct vdp {
	union vdp_regs regs;
	union status status;
	uint8_t code:2;
	uint16_t address:14;
	uint8_t read_buffer;
	uint8_t cmd_byte;
	bool cmd_first_write;
	int v_counter;
	uint8_t bg_x_scroll;
	uint8_t bg_y_scroll;
	uint8_t line_counter;
	struct clock clock;
	uint8_t vram[VRAM_SIZE];
	uint8_t cram[CRAM_SIZE];
	int bus_id;
	int irq;
	struct port_region region;
};

static bool vdp_init(struct controller_instance *instance);
static void vdp_tick(struct vdp *vdp);
static void vdp_deinit(struct controller_instance *instance);
static void vdp_draw_line_bg(struct vdp *vdp);
static uint8_t vdp_read(struct vdp *vdp, port_t port);
static void vdp_write(struct vdp *vdp, uint8_t b, port_t port);
static uint8_t ctrl_read(struct vdp *vdp);
static void ctrl_write(struct vdp *vdp, uint8_t b);
static uint8_t data_read(struct vdp *vdp);
static void data_write(struct vdp *vdp, uint8_t b);

static struct pops vdp_pops = {
	.read = (read_t)vdp_read,
	.write = (write_t)vdp_write
};

uint8_t ctrl_read(struct vdp *vdp)
{
	uint8_t status;

	/* Reset first write flag */
	vdp->cmd_first_write = true;

	/* Clear and return status register */
	status = vdp->status.raw;
	vdp->status.raw = 0;
	vdp->status.reserved = 0x1F;
	return status;
}

void ctrl_write(struct vdp *vdp, uint8_t b)
{
	union cmd_word cmd_word;

	/* Handle first write */
	if (vdp->cmd_first_write) {
		vdp->cmd_byte = b;
		vdp->cmd_first_write = false;
		return;
	}

	/* Reset first write flag */
	vdp->cmd_first_write = true;

	/* Build command word */
	cmd_word.raw = vdp->cmd_byte | (b << 8);

	/* Save code and address registers */
	vdp->code = cmd_word.code;
	vdp->address = cmd_word.address;

	/* Handle write based on code register */
	switch (vdp->code) {
	case 0:
		/* A byte of VRAM is read from the location defined by the
		address register and is stored in the read buffer. The
		address register is incremented by one. */
		vdp->read_buffer = vdp->vram[vdp->address++ & (VRAM_SIZE - 1)];
		break;
	case 2:
		/* This value signifies a VDP register write */
		vdp->regs.raw[cmd_word.reg] = cmd_word.data;
		break;
	default:
		break;
	}
}

uint8_t data_read(struct vdp *vdp)
{
	uint8_t b;

	/* Reads from VRAM are buffered. Every time the data port is read
	(regardless of the code regsister) the contents of a buffer are
	returned. The VDP will then read a byte from VRAM at the current
	address, and increment the address register. */
	b = vdp->read_buffer;
	vdp->read_buffer = vdp->vram[vdp->address++ & (VRAM_SIZE - 1)];
	return b;
}

void data_write(struct vdp *vdp, uint8_t b)
{
	/* Depending on the code register, data written to the data port is sent
	to either VRAM or CRAM. After each write, the address register is
	incremented by one, and will wrap past $3FFF. */
	switch (vdp->code) {
	case 0:
	case 1:
	case 2:
		vdp->vram[vdp->address++ & (VRAM_SIZE - 1)] = b;
		break;
	case 3:
		vdp->cram[vdp->address++ & (CRAM_SIZE - 1)] = b;
		break;
	}

	/* An additional quirk is that writing to the data port will also load
	the buffer with the value written. */
	vdp->read_buffer = b;
}

uint8_t vdp_read(struct vdp *vdp, port_t port)
{
	/* Call appropriate port read function */
	switch (port) {
	case DATA_PORT:
		return data_read(vdp);
	case CTRL_PORT:
		return ctrl_read(vdp);
	default:
		break;
	}

	return 0;
}

void vdp_write(struct vdp *vdp, uint8_t b, port_t port)
{
	/* Call appropriate port write function */
	switch (port) {
	case DATA_PORT:
		data_write(vdp, b);
		break;
	case CTRL_PORT:
		ctrl_write(vdp, b);
		break;
	default:
		break;
	}
}

void vdp_draw_line_bg(struct vdp *vdp)
{
	union vdp_addr vdp_addr;
	struct color color;
	union bg_tile tile;
	uint16_t tile_data_addr;
	uint16_t x;
	uint8_t final_x;
	uint16_t final_y;
	uint8_t tile_data;
	uint8_t palette_index;
	uint8_t col;
	uint8_t row;
	uint8_t x_off;
	uint8_t y_off;
	uint8_t bit;
	uint8_t v;
	int i;

	/* Find final Y coordinate based on vertical scroll */
	final_y = vdp->v_counter + vdp->regs.bg_y_scroll;

	/* Wrap Y coordinate if needed */
	if (final_y >= MAX_BG_Y)
		final_y -= MAX_BG_Y;

	/* Get row */
	row = final_y / TILE_HEIGHT;

	/* The SMS VDP will logically AND bit 0 with the VDP address, meaning
	if bit 0 is cleared, bit 4 of the name table row is forced to zero. When
	the screen is displayed, this causes the lower 8 rows to mirror the top
	16 rows. */
	bit = bitops_getb(&row, 4, 1) & vdp->regs.name_table_base_addr.bit0;
	bitops_setb(&row, 4, 1, bit);

	/* Draw line */
	for (x = 0; x < SCREEN_WIDTH; x++) {
		/* Handle display blanking */
		if (!vdp->regs.mode_ctrl_2.enable_display) {
			color.r = 0;
			color.g = 0;
			color.b = 0;
			video_set_pixel(x, vdp->v_counter, color);
			continue;
		}

		/* Mask column 0 with overscan color if needed */
		if (vdp->regs.mode_ctrl_1.mask_col_0 && (x < TILE_WIDTH)) {
			palette_index = vdp->regs.overscan_color.color;
			v = vdp->cram[palette_index + SPRITE_PALETTE_OFFSET];
			color.r = RED(v);
			color.g = GREEN(v);
			color.b = BLUE(v);
			video_set_pixel(x, vdp->v_counter, color);
			continue;
		}

		/* Find final X coordinate based on horizontal scroll */
		final_x = (vdp->regs.mode_ctrl_1.hori_scroll_lock &&
			(vdp->v_counter < HORI_SCROLL_LOCK_HEIGHT)) ?
			x : x - vdp->regs.bg_x_scroll;

		/* Get column */
		col = final_x / TILE_WIDTH;

		/* Set final VDP address */
		vdp_addr.unused = 0;
		vdp_addr.sel = vdp->regs.name_table_base_addr.addr;
		vdp_addr.row = row;
		vdp_addr.column = col;
		vdp_addr.word = 0;

		/* Get tile index and flags */
		tile.low = vdp->vram[vdp_addr.raw];
		tile.high = vdp->vram[vdp_addr.raw + 1];

		/* Set X offset based on X coordinate and horizontal flip */
		x_off = TILE_WIDTH - 1 - (final_x % TILE_WIDTH);
		if (tile.h_flip)
			x_off = (final_x % TILE_WIDTH);

		/* Set Y offset based on X coordinate and vertical flip */
		y_off = final_y % TILE_HEIGHT;
		if (tile.v_flip)
			y_off = TILE_HEIGHT - 1 - y_off;

		/* Compute tile data address from pattern index and Y offset */
		tile_data_addr = tile.pattern_index * TILE_SIZE;
		tile_data_addr += y_off * (TILE_SIZE / TILE_WIDTH);

		/* Compute palette index from bit planes */
		palette_index = 0;
		for (i = 0; i < NUM_BIT_PLANES; i++) {
			tile_data = vdp->vram[tile_data_addr++];
			bit = (tile_data >> x_off) & 1;
			bitops_setb(&palette_index, i, 1, bit);
		}

		/* Switch to second (sprite) palette if needed */
		if (tile.palette_sel)
			palette_index += SPRITE_PALETTE_OFFSET;

		/* Compute color based on palette index and draw pixel */
		v = vdp->cram[palette_index];
		color.r = RED(v);
		color.g = GREEN(v);
		color.b = BLUE(v);
		video_set_pixel(x, vdp->v_counter, color);
	}
}

void vdp_tick(struct vdp *vdp)
{
	/* Draw current line if within bounds */
	if (vdp->v_counter < SCREEN_HEIGHT) {
		video_lock();
		vdp_draw_line_bg(vdp);
		video_unlock();
	}

	/* Handle line counter */
	if (vdp->v_counter <= SCREEN_HEIGHT) {
		vdp->line_counter--;
		if (vdp->line_counter == 0xFF) {
			vdp->line_counter = vdp->regs.line_counter;
			if (vdp->regs.mode_ctrl_1.enable_line_interrupts)
				cpu_interrupt(vdp->irq);
		}
	}

	/* Check if frame is complete */
	if (vdp->v_counter == SCREEN_HEIGHT) {
		/* Update display */
		video_update();

		/* Handle VSYNC */
		vdp->status.frame_interrupt_pending = 1;
		if (vdp->regs.mode_ctrl_2.enable_frame_interrupts)
			cpu_interrupt(vdp->irq);
	}

	/* The line counter is loaded on every line outside of the active
	display period excluding the line after the last line of the active
	display period. */
	if (vdp->v_counter > SCREEN_HEIGHT)
		vdp->line_counter = vdp->regs.line_counter;

	/* Update vertical counter and latch BG Y scroll */
	if (++vdp->v_counter == NUM_ROWS) {
		vdp->v_counter = 0;
		vdp->bg_y_scroll = vdp->regs.bg_y_scroll;
	}

	/* Consume an entire row of clock cycles (each pixel takes 2 cycles) */
	clock_consume(NUM_COLUMNS * 2);
}

bool vdp_init(struct controller_instance *instance)
{
	struct vdp *vdp;
	struct video_specs video_specs;
	struct resource *res;

	/* Initialize video frontend */
	video_specs.width = SCREEN_WIDTH;
	video_specs.height = SCREEN_HEIGHT;
	video_specs.fps = SCREEN_REFRESH_RATE;
	if (!video_init(&video_specs))
		return false;

	/* Allocate VDP structure */
	instance->priv_data = malloc(sizeof(struct vdp));
	vdp = instance->priv_data;

	/* Save bus ID for later use */
	vdp->bus_id = instance->bus_id;

	/* Add VDP port region */
	res = resource_get("port",
		RESOURCE_PORT,
		instance->resources,
		instance->num_resources);
	vdp->region.area = res;
	vdp->region.pops = &vdp_pops;
	vdp->region.data = vdp;
	port_region_add(&vdp->region);

	/* Get IRQ number */
	res = resource_get("irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	vdp->irq = res->data.irq;

	/* Set up clock */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	vdp->clock.rate = res->data.clk;
	vdp->clock.data = vdp;
	vdp->clock.tick = (clock_tick_t)vdp_tick;
	clock_add(&vdp->clock);

	return true;
}

void vdp_reset(struct controller_instance *instance)
{
	struct vdp *vdp = instance->priv_data;

	/* Initialize data */
	memset(&vdp->regs, 0, sizeof(union vdp_regs));
	memset(vdp->vram, 0, VRAM_SIZE);
	vdp->address = 0;
	vdp->status.raw = 0;
	vdp->status.reserved = 0x1F;
	vdp->cmd_first_write = true;
	vdp->v_counter = 0;
	vdp->line_counter = 0xFF;
	vdp->bg_x_scroll = 0;
	vdp->bg_y_scroll = 0;

	/* Enable clock */
	vdp->clock.enabled = true;
}

void vdp_deinit(struct controller_instance *instance)
{
	video_deinit();
	free(instance->priv_data);
}

CONTROLLER_START(vdp)
	.init = vdp_init,
	.reset = vdp_reset,
	.deinit = vdp_deinit
CONTROLLER_END

