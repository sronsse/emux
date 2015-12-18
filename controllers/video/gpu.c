#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <bitops.h>
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
#define FB_W		1024
#define FB_H		512

#define STP_HALF	0
#define STP_ADD		1
#define STP_SUB		2
#define STP_QUARTER	3

#define TEX_4BIT	0
#define TEX_8BIT	1
#define TEX_15BIT	2

#define GPU_TYPE	2

#define DMA_DIR_OFF		0
#define DMA_DIR_FIFO_STATUS	1
#define DMA_DIR_CPU_TO_GP0	2
#define DMA_DIR_GPUREAD_TO_CPU	3

struct render_data {
	uint8_t tex_page_x;
	uint8_t tex_page_y;
	uint8_t tex_page_colors;
	uint8_t clut_x;
	uint16_t clut_y;
	uint8_t semi_transparency;
	bool opaque;
	bool textured;
	bool raw;
	bool dithering;
};

struct pixel {
	int16_t x;
	int16_t y;
	struct color c;
	uint8_t u;
	uint8_t v;
	struct render_data *render_data;
};

struct line {
	float x1;
	float x2;
	float y;
	struct color c1;
	struct color c2;
	float u1;
	float u2;
	float v;
	struct render_data *render_data;
};

struct triangle {
	float x1;
	float y1;
	float x2;
	float y2;
	float x3;
	float y3;
	struct color c1;
	struct color c2;
	struct color c3;
	float u1;
	float v1;
	float u2;
	float v2;
	float u3;
	float v3;
	struct render_data *render_data;
};

struct rectangle {
	int16_t x;
	int16_t y;
	uint16_t width;
	uint16_t height;
	struct color c;
	uint8_t u;
	uint8_t v;
	struct render_data *render_data;
};

struct ray {
	int16_t x1;
	int16_t y1;
	int16_t x2;
	int16_t y2;
	struct color c1;
	struct color c2;
	struct render_data *render_data;
};

union vertex {
	uint32_t raw;
	struct {
		int32_t x_coord:11;
		int32_t unused1:5;
		int32_t y_coord:11;
		int32_t unused2:5;
	};
};

union color_attr {
	uint32_t raw;
	struct {
		uint32_t red:8;
		uint32_t green:8;
		uint32_t blue:8;
		uint32_t unused:8;
	};
};

struct tex_page_attr {
	uint16_t x_base:4;
	uint16_t y_base:1;
	uint16_t semi_transparency:2;
	uint16_t colors:2;
	uint16_t unused1:2;
	uint16_t tex_disable:1;
	uint16_t unused2:2;
	uint16_t unused3:2;
};

struct clut_attr {
	uint16_t x_coord:6;
	uint16_t y_coord:9;
	uint16_t unused:1;
};

struct tex_coords {
	uint8_t u;
	uint8_t v;
};

struct dimensions {
	uint32_t width:16;
	uint32_t height:16;
};

union cmd_fill_rectangle {
	uint32_t raw[3];
	struct {
		union color_attr color;
		union vertex top_left;
		struct dimensions dimensions;
	};
};

union cmd_monochrome_poly {
	uint32_t raw[5];
	struct {
		union color_attr color;
		union vertex v1;
		union vertex v2;
		union vertex v3;
		union vertex v4;
	};
};

union cmd_textured_poly {
	uint32_t raw[9];
	struct {
		union color_attr color;
		union vertex v1;
		struct {
			struct tex_coords tex_coords1;
			struct clut_attr palette;
		};
		union vertex v2;
		struct {
			struct tex_coords tex_coords2;
			struct tex_page_attr tex_page;
		};
		union vertex v3;
		struct {
			struct tex_coords tex_coords3;
			uint16_t unused1;
		};
		union vertex v4;
		struct {
			struct tex_coords tex_coords4;
			uint16_t unused2;
		};
	};
};

union cmd_shaded_poly {
	uint32_t raw[8];
	struct {
		union color_attr color1;
		union vertex v1;
		union color_attr color2;
		union vertex v2;
		union color_attr color3;
		union vertex v3;
		union color_attr color4;
		union vertex v4;
	};
};

union cmd_shaded_textured_poly {
	uint32_t raw[12];
	struct {
		union color_attr color1;
		union vertex v1;
		struct {
			struct tex_coords tex_coords1;
			struct clut_attr palette;
		};
		union color_attr color2;
		union vertex v2;
		struct {
			struct tex_coords tex_coords2;
			struct tex_page_attr tex_page;
		};
		union color_attr color3;
		union vertex v3;
		struct {
			struct tex_coords tex_coords3;
			uint16_t unused1;
		};
		union color_attr color4;
		union vertex v4;
		struct {
			struct tex_coords tex_coords4;
			uint16_t unused2;
		};
	};
};

union cmd_monochrome_line {
	uint32_t raw[3];
	struct {
		union color_attr color;
		union vertex v1;
		union vertex v2;
	};
};

union cmd_shaded_line {
	uint32_t raw[4];
	struct {
		union color_attr color1;
		union vertex v1;
		union color_attr color2;
		union vertex v2;
	};
};

union cmd_monochrome_rect {
	uint32_t raw[3];
	struct {
		union color_attr color;
		union vertex top_left;
		struct dimensions dimensions;
	};
};

union cmd_textured_rect {
	uint32_t raw[4];
	struct {
		union color_attr color;
		union vertex top_left;
		struct {
			struct tex_coords tex_coords;
			struct clut_attr palette;
		};
		struct dimensions dimensions;
	};
};

union cmd_copy_rect_vram_to_vram {
	uint32_t raw[4];
	struct {
		uint32_t command;
		union vertex src;
		union vertex dest;
		struct dimensions dimensions;
	};
};

union cmd_copy_rect {
	uint32_t raw[3];
	struct {
		uint32_t command;
		union vertex xy;
		struct dimensions dimensions;
	};
};

struct cmd_draw_mode_setting {
	uint32_t x_base:4;
	uint32_t y_base:1;
	uint32_t semi_transparency:2;
	uint32_t colors:2;
	uint32_t dither:1;
	uint32_t drawing_allowed:1;
	uint32_t disable:1;
	uint32_t rect_x_flip:1;
	uint32_t rect_y_flip:1;
	uint32_t unused:10;
	uint32_t opcode:8;
};

struct cmd_tex_window_setting {
	uint32_t mask_x:5;
	uint32_t mask_y:5;
	uint32_t offset_x:5;
	uint32_t offset_y:5;
	uint32_t unused:4;
	uint32_t opcode:8;
};

struct cmd_set_drawing_area {
	uint32_t x_coord:10;
	uint32_t y_coord:10;
	uint32_t unused:4;
	uint32_t opcode:8;
};

struct cmd_set_drawing_offset {
	int32_t x_offset:11;
	int32_t y_offset:11;
	uint32_t unused:2;
	uint32_t opcode:8;
};

struct cmd_mask_bit_setting {
	uint32_t set_while_drawing:1;
	uint32_t check_before_draw:1;
	uint32_t unused:22;
	uint32_t opcode:8;
};

struct cmd_display_enable {
	uint32_t display_on_off:1;
	uint32_t unused:23;
	uint32_t opcode:8;
};

struct cmd_dma_dir {
	uint32_t dir:2;
	uint32_t unused:22;
	uint32_t opcode:8;
};

struct cmd_start_of_display_area {
	uint32_t x:10;
	uint32_t y:9;
	uint32_t unused:5;
	uint32_t opcode:8;
};

struct cmd_horizontal_display_range {
	uint32_t x1:12;
	uint32_t x2:12;
	uint32_t opcode:8;
};

struct cmd_vertical_display_range {
	uint32_t y1:10;
	uint32_t y2:10;
	uint32_t unused:4;
	uint32_t opcode:8;
};

struct cmd_display_mode {
	uint32_t horizontal_res_1:2;
	uint32_t vertical_res:1;
	uint32_t video_mode:1;
	uint32_t color_depth:1;
	uint32_t vertical_interlace:1;
	uint32_t horizontal_res_2:1;
	uint32_t reverse:1;
	uint32_t unused:16;
	uint32_t opcode:8;
};

struct cmd_texture_disable {
	uint32_t disable:1;
	uint32_t unused:23;
	uint32_t opcode:8;
};

struct cmd_get_gpu_info {
	uint32_t info:4;
	uint32_t unused:20;
	uint32_t opcode:8;
};

union cmd {
	uint32_t raw;
	struct {
		uint32_t data:24;
		uint32_t opcode:8;
	};
	struct cmd_draw_mode_setting draw_mode_setting;
	struct cmd_tex_window_setting tex_window_setting;
	struct cmd_set_drawing_area set_drawing_area;
	struct cmd_set_drawing_offset set_drawing_offset;
	struct cmd_mask_bit_setting mask_bit_setting;
	struct cmd_display_enable display_enable;
	struct cmd_dma_dir dma_dir;
	struct cmd_start_of_display_area start_of_display_area;
	struct cmd_horizontal_display_range horizontal_display_range;
	struct cmd_vertical_display_range vertical_display_range;
	struct cmd_display_mode display_mode;
	struct cmd_texture_disable texture_disable;
	struct cmd_get_gpu_info get_gpu_info;
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

static void draw_pixel(struct gpu *gpu, struct pixel *pixel);
static void draw_line(struct gpu *gpu, struct line *line);
static void draw_triangle_flat_bottom(struct gpu *gpu, struct triangle *tri);
static void draw_triangle_flat_top(struct gpu *gpu, struct triangle *tri);
static void draw_triangle(struct gpu *gpu, struct triangle *tri);
static void draw_rectangle(struct gpu *gpu, struct rectangle *rectangle);
static void draw_ray(struct gpu *gpu, struct ray *ray);

static inline bool fifo_empty(struct fifo *fifo);
static inline bool fifo_full(struct fifo *fifo);
static inline uint8_t fifo_cmd(struct fifo *fifo);
static inline bool fifo_enqueue(struct fifo *fifo, uint32_t data);
static inline bool fifo_dequeue(struct fifo *fifo, uint32_t *data, int size);

static inline void cmd_nop();
static inline void cmd_clear_cache(struct gpu *gpu);
static inline void cmd_fill_rectangle(struct gpu *gpu);
static inline void cmd_monochrome_3p_poly(struct gpu *gpu, bool opaque);
static inline void cmd_monochrome_4p_poly(struct gpu *gpu, bool opaque);
static inline void cmd_textured_3p_poly(struct gpu *gpu, bool opaque, bool raw);
static inline void cmd_textured_4p_poly(struct gpu *gpu, bool opaque, bool raw);
static inline void cmd_shaded_3p_poly(struct gpu *gpu, bool opaque);
static inline void cmd_shaded_4p_poly(struct gpu *gpu, bool opaque);
static inline void cmd_shaded_textured_3p_poly(struct gpu *gpu, bool opaque);
static inline void cmd_shaded_textured_4p_poly(struct gpu *gpu, bool opaque);
static inline void cmd_monochrome_line(struct gpu *gpu, bool opaque);
static inline void cmd_monochrome_poly_line(struct gpu *gpu, bool opaque);
static inline void cmd_shaded_line(struct gpu *gpu, bool opaque);
static inline void cmd_shaded_poly_line(struct gpu *gpu, bool opaque);
static inline void cmd_monochrome_rect_var(struct gpu *gpu, bool opaque);
static inline void cmd_monochrome_rect_1x1(struct gpu *gpu, bool opaque);
static inline void cmd_monochrome_rect_8x8(struct gpu *gpu, bool opaque);
static inline void cmd_monochrome_rect_16x16(struct gpu *gpu, bool opaque);
static inline void cmd_tex_rect_var(struct gpu *gpu, bool opaque, bool raw);
static inline void cmd_tex_rect_1x1(struct gpu *gpu, bool opaque, bool raw);
static inline void cmd_tex_rect_8x8(struct gpu *gpu, bool opaque, bool raw);
static inline void cmd_tex_rect_16x16(struct gpu *gpu, bool opaque, bool raw);
static inline void cmd_copy_rect_vram_to_vram(struct gpu *gpu);
static inline void cmd_copy_rect_cpu_to_vram(struct gpu *gpu);
static inline void cmd_copy_rect_vram_to_cpu(struct gpu *gpu);
static inline void cmd_draw_mode_setting(struct gpu *gpu, union cmd cmd);
static inline void cmd_tex_window_setting(struct gpu *gpu, union cmd cmd);
static inline void cmd_set_drawing_area_tl(struct gpu *gpu, union cmd cmd);
static inline void cmd_set_drawing_area_br(struct gpu *gpu, union cmd cmd);
static inline void cmd_set_drawing_offset(struct gpu *gpu, union cmd cmd);
static inline void cmd_mask_bit_setting(struct gpu *gpu, union cmd cmd);

static inline void cmd_reset_gpu(struct gpu *gpu);
static inline void cmd_reset_cmd_buffer(struct gpu *gpu);
static inline void cmd_ack_interrupt(struct gpu *gpu);
static inline void cmd_display_enable(struct gpu *gpu, union cmd cmd);
static inline void cmd_dma_dir(struct gpu *gpu, union cmd cmd);
static inline void cmd_start_of_display_area(struct gpu *gpu, union cmd cmd);
static inline void cmd_horizontal_display_range(struct gpu *gpu, union cmd cmd);
static inline void cmd_vertical_display_range(struct gpu *gpu, union cmd cmd);
static inline void cmd_display_mode(struct gpu *gpu, union cmd cmd);
static inline void cmd_texture_disable(struct gpu *gpu, union cmd cmd);
static inline void cmd_get_gpu_info(struct gpu *gpu, union cmd cmd);

static struct mops gpu_mops = {
	.readl = (readl_t)gpu_readl,
	.writel = (writel_t)gpu_writel
};

static struct dma_ops gpu_dma_ops = {
	.readl = (dma_readl_t)gpu_dma_readl,
	.writel = (dma_writel_t)gpu_dma_writel
};

static int8_t dither_pattern[] = {
	-4, 0, -3, 1,
	2, -2, 3, -1,
	-3, 1, -4, 0,
	3, -1, 2, -2
};

void draw_pixel(struct gpu *gpu, struct pixel *pixel)
{
	struct render_data *render_data = pixel->render_data;
	struct color dest;
	uint16_t data;
	uint16_t bg;
	uint32_t pixel_off;
	uint32_t clut_off;
	uint32_t tex_off;
	uint8_t index;
	int16_t r;
	int16_t g;
	int16_t b;
	uint8_t u;
	uint8_t v;
	int8_t dither;
	int dither_x;
	int dither_y;
	bool opaque;

	/* Add drawing offset to pixel coordinates */
	pixel->x += gpu->drawing_offset_x;
	pixel->y += gpu->drawing_offset_y;

	/* Discard pixel if it is outside the frame buffer */
	if ((pixel->x < 0) ||
		(pixel->y < 0) ||
		(pixel->x >= FB_W) ||
		(pixel->y >= FB_H))
		return;

	/* Discard pixel if it is outside of the clip area */
	if ((pixel->x < gpu->drawing_area_x1) ||
		(pixel->y < gpu->drawing_area_y1) ||
		(pixel->x > gpu->drawing_area_x2) ||
		(pixel->y > gpu->drawing_area_y2))
		return;

	/* Compute pixel offset */
	pixel_off = (pixel->x + pixel->y * FB_W) * sizeof(uint16_t);

	/* Get existing pixel data within frame buffer */
	bg = (gpu->vram[pixel_off] << 8) | gpu->vram[pixel_off + 1];

	/* Stop processing if mask out mode is on and pixel is masked (MSB) */
	if (gpu->stat.draw_pixels && bitops_getw(&bg, 15, 1))
		return;

	/* Set initial color components and data */
	r = pixel->c.r;
	g = pixel->c.g;
	b = pixel->c.b;
	data = 0;

	/* Set opaque flag based on render data */
	opaque = render_data->opaque;

	/* Check if texture data needs to be sampled */
	if (render_data->textured && !gpu->stat.tex_disable) {
		/* Adapt texture coordinates based on texture window as such:
		(Texcoord AND (NOT (Mask * 8))) OR ((Offset AND Mask) * 8) */
		u = pixel->u & ~(gpu->tex_window_mask_x * 8);
		u |= (gpu->tex_window_offset_x & gpu->tex_window_mask_x) * 8;
		v = pixel->v & ~(gpu->tex_window_mask_y * 8);
		v |= (gpu->tex_window_offset_y & gpu->tex_window_mask_y) * 8;

		/* Set palette offset (unused for 15-bit textures) */
		clut_off = render_data->clut_x * 16;
		clut_off += render_data->clut_y * FB_W;
		clut_off *= sizeof(uint16_t);

		/* Set texel offset based on texture page and V coordinate */
		tex_off = render_data->tex_page_x * 64;
		tex_off += (render_data->tex_page_y * 256 + v) * FB_W;
		tex_off *= sizeof(uint16_t);

		/* Set final texel offset based on texture bits per pixel */
		switch (render_data->tex_page_colors) {
		case TEX_4BIT:
			/* Update offset (each texel is 4-bit wide) */
			tex_off += (u / 4) * sizeof(uint16_t);

			/* Set palette index (get appropriate nibble) */
			data = gpu->vram[tex_off] << 8;
			data |= gpu->vram[tex_off + 1];
			index = bitops_getw(&data, (u % 4) * 4, 4);

			/* Compute final texture offset */
			tex_off = clut_off + index * sizeof(uint16_t);
			break;
		case TEX_8BIT:
			/* Update offset (each texel is 8-bit wide) */
			tex_off += (u / 2) * sizeof(uint16_t);

			/* Set palette index (get appropriate byte) */
			data = gpu->vram[tex_off] << 8;
			data |= gpu->vram[tex_off + 1];
			index = bitops_getw(&data, (u % 2) * 8, 8);

			/* Compute final texture offset */
			tex_off = clut_off + index * sizeof(uint16_t);
			break;
		case TEX_15BIT:
		default:
			/* Update offset (each texel is 16-bit wide) */
			tex_off += u * sizeof(uint16_t);
			break;
		}

		/* Get final texel data and discard pixel if texel is 0 */
		data = (gpu->vram[tex_off] << 8) | gpu->vram[tex_off + 1];
		if (data == 0)
			return;

		/* Update opaque state based on texel STP bit if needed */
		if (!opaque)
			opaque = !bitops_getw(&data, 15, 1);

		/* Extract texel color components */
		r = bitops_getw(&data, 0, 5) << 3;
		g = bitops_getw(&data, 5, 5) << 3;
		b = bitops_getw(&data, 10, 5) << 3;

		/* Blend color if needed (8bit values of 0x80 are brightest and
		values 0x81..0xFF are "brighter than bright" allowing to make
		textures about twice brighter than they are) */
		if (!render_data->raw) {
			/* Blend pixel */
			r = (r * pixel->c.r) / 0x80;
			g = (g * pixel->c.g) / 0x80;
			b = (b * pixel->c.b) / 0x80;

			/* Clamp result */
			r = (r >= 0x00) ? ((r <= 0xFF) ? r : 0xFF) : 0x00;
			g = (g >= 0x00) ? ((g <= 0xFF) ? g : 0xFF) : 0x00;
			b = (b >= 0x00) ? ((b <= 0xFF) ? b : 0xFF) : 0x00;
		}
	}

	/* Handle dithering if requested */
	if (render_data->dithering) {
		/* Get dither value from 4x4 dither pattern */
		dither_x = pixel->x & 0x03;
		dither_y = pixel->y & 0x03;
		dither = dither_pattern[dither_x + dither_y * 4];

		/* Set color components */
		r += dither;
		g += dither;
		b += dither;

		/* Clamp result */
		r = (r >= 0x00) ? ((r <= 0xFF) ? r : 0xFF) : 0x00;
		g = (g >= 0x00) ? ((g <= 0xFF) ? g : 0xFF) : 0x00;
		b = (b >= 0x00) ? ((b <= 0xFF) ? b : 0xFF) : 0x00;
	}

	/* Convert pixel from 24-bit to 15-bit */
	r >>= 3;
	g >>= 3;
	b >>= 3;

	/* Check if semi-transparency mode is requested */
	if (!opaque) {
		/* Extract color components from existing data */
		dest.r = bitops_getw(&bg, 0, 5);
		dest.g = bitops_getw(&bg, 5, 5);
		dest.b = bitops_getw(&bg, 10, 5);

		/* Handle semi-transparency operation */
		switch (render_data->semi_transparency) {
		case STP_HALF:
			r = (dest.r + r) / 2;
			g = (dest.g + g) / 2;
			b = (dest.b + b) / 2;
			break;
		case STP_ADD:
			r += dest.r;
			g += dest.g;
			b += dest.b;
			break;
		case STP_SUB:
			r = dest.r - r;
			g = dest.g - g;
			b = dest.b - b;
			break;
		case STP_QUARTER:
		default:
			r = dest.r + (r / 4);
			g = dest.g + (g / 4);
			b = dest.b + (b / 4);
			break;
		}

		/* Clamp result */
		r = (r >= 0x00) ? ((r <= 0x1F) ? r : 0x1F) : 0x00;
		g = (g >= 0x00) ? ((g <= 0x1F) ? g : 0x1F) : 0x00;
		b = (b >= 0x00) ? ((b <= 0x1F) ? b : 0x1F) : 0x00;
	}

	/* Fill color components */
	bitops_setw(&data, 0, 5, r);
	bitops_setw(&data, 5, 5, g);
	bitops_setw(&data, 10, 5, b);

	/* Force masking bit (MSB) if required */
	if (gpu->stat.set_mask_bit)
		bitops_setw(&data, 15, 1, 1);

	/* Write data to frame buffer */
	gpu->vram[pixel_off] = data >> 8;
	gpu->vram[pixel_off + 1] = data;
}

void draw_line(struct gpu *gpu, struct line *line)
{
	struct pixel pixel;
	float x;
	struct color c;
	uint8_t u;
	float f;

	/* Copy render data */
	pixel.render_data = line->render_data;

	/* Draw line */
	for (x = floor(line->x1); x < line->x2; x++) {
		/* Get interpolation factor */
		f = fmaxf((x - line->x1) / (line->x2 - line->x1), 0.0f);

		/* Interpolate color */
		c.r = line->c1.r + f * (line->c2.r - line->c1.r);
		c.g = line->c1.g + f * (line->c2.g - line->c1.g);
		c.b = line->c1.b + f * (line->c2.b - line->c1.b);

		/* Interpolate texture U coordinate */
		u = line->u1 + f * (line->u2 - line->u1);

		/* Build and draw pixel */
		pixel.x = x;
		pixel.y = line->y;
		pixel.c = c;
		pixel.u = u;
		pixel.v = line->v;
		draw_pixel(gpu, &pixel);
	}
}

void draw_triangle_flat_bottom(struct gpu *gpu, struct triangle *triangle)
{
	float x;
	float y;
	struct color c;
	float u;
	float v;
	struct line l;
	float x1;
	float x2;
	float x1_inc;
	float x2_inc;
	float t1;
	float t2;
	float t3;
	float t4;
	float f;

	/* Swap second and third vertices if needed */
	if (triangle->x3 < triangle->x2) {
		x = triangle->x2;
		y = triangle->y2;
		c = triangle->c2;
		u = triangle->u2;
		v = triangle->v2;
		triangle->x2 = triangle->x3;
		triangle->y2 = triangle->y3;
		triangle->c2 = triangle->c3;
		triangle->u2 = triangle->u3;
		triangle->v2 = triangle->v3;
		triangle->x3 = x;
		triangle->y3 = y;
		triangle->c3 = c;
		triangle->u3 = u;
		triangle->v3 = v;
	}

	/* Calculate line X increments */
	x1_inc = triangle->x2 - triangle->x1;
	x1_inc /= triangle->y2 - triangle->y1;
	x2_inc = triangle->x3 - triangle->x1;
	x2_inc /= triangle->y3 - triangle->y1;

	/* Initialize X coordinates to first vertex X coordinate */
	x1 = triangle->x1;
	x2 = triangle->x1;

	/* Copy render data */
	l.render_data = triangle->render_data;

	/* Draw triangle */
	for (y = triangle->y1; y < triangle->y2; y++) {
		/* Set line coordinates */
		l.x1 = x1;
		l.x2 = x2;
		l.y = y;

		/* Compute first line vertex interpolation factor */
		t1 = triangle->x1 - x1;
		t2 = triangle->y1 - y;
		t3 = triangle->x2 - triangle->x1;
		t4 = triangle->y2 - triangle->y1;
		f = sqrtf((t1 * t1) + (t2 * t2));
		if ((t3 != 0.0f) || (t4 != 0.0f))
			f /= sqrtf((t3 * t3) + (t4 * t4));

		/* Compute first vertex color */
		l.c1.r = triangle->c1.r + f * (triangle->c2.r - triangle->c1.r);
		l.c1.g = triangle->c1.g + f * (triangle->c2.g - triangle->c1.g);
		l.c1.b = triangle->c1.b + f * (triangle->c2.b - triangle->c1.b);

		/* Compute first vertex texture U coordinate */
		l.u1 = triangle->u1 + f * (triangle->u2 - triangle->u1);

		/* Compute second line vertex interpolation factor */
		t1 = triangle->x1 - x2;
		t2 = triangle->y1 - y;
		t3 = triangle->x3 - triangle->x1;
		t4 = triangle->y3 - triangle->y1;
		f = sqrtf((t1 * t1) + (t2 * t2));
		if ((t3 != 0.0f) || (t4 != 0.0f))
			f /= sqrtf((t3 * t3) + (t4 * t4));

		/* Compute second vertex color */
		l.c2.r = triangle->c1.r + f * (triangle->c3.r - triangle->c1.r);
		l.c2.g = triangle->c1.g + f * (triangle->c3.g - triangle->c1.g);
		l.c2.b = triangle->c1.b + f * (triangle->c3.b - triangle->c1.b);

		/* Compute second vertex texture U coordinate */
		l.u2 = triangle->u1 + f * (triangle->u3 - triangle->u1);

		/* Compute line texture V coordinate */
		f = y - triangle->y1;
		if (triangle->y1 != triangle->y2)
			f /= triangle->y2 - triangle->y1;
		l.v = triangle->v1 + f * (triangle->v2 - triangle->v1);

		/* Update X coordinates */
		x1 += x1_inc;
		x2 += x2_inc;

		/* Draw single line */
		draw_line(gpu, &l);
	}
}

void draw_triangle_flat_top(struct gpu *gpu, struct triangle *triangle)
{
	float x;
	float y;
	struct color c;
	float u;
	float v;
	struct line l;
	float x1;
	float x2;
	float x1_inc;
	float x2_inc;
	float t1;
	float t2;
	float t3;
	float t4;
	float f;

	/* Swap first and second vertices if needed */
	if (triangle->x2 < triangle->x1) {
		x = triangle->x1;
		y = triangle->y1;
		c = triangle->c1;
		u = triangle->u1;
		v = triangle->v1;
		triangle->x1 = triangle->x2;
		triangle->y1 = triangle->y2;
		triangle->c1 = triangle->c2;
		triangle->u1 = triangle->u2;
		triangle->v1 = triangle->v2;
		triangle->x2 = x;
		triangle->y2 = y;
		triangle->c2 = c;
		triangle->u2 = u;
		triangle->v2 = v;
	}

	/* Calculate line X increments */
	x1_inc = triangle->x1 - triangle->x3;
	x1_inc /= triangle->y1 - triangle->y3;
	x2_inc = triangle->x2 - triangle->x3;
	x2_inc /= triangle->y2 - triangle->y3;

	/* Initialize X coordinates to third vertex X coordinate */
	x1 = triangle->x3;
	x2 = triangle->x3;

	/* Copy render data */
	l.render_data = triangle->render_data;

	/* Draw triangle */
	for (y = triangle->y3; y >= triangle->y1; y--) {
		/* Set line coordinates */
		l.x1 = x1;
		l.x2 = x2;
		l.y = y;

		/* Compute first line interpolation factor */
		t1 = triangle->x3 - x1;
		t2 = triangle->y3 - y;
		t3 = triangle->x3 - triangle->x1;
		t4 = triangle->y3 - triangle->y1;
		f = sqrtf((t1 * t1) + (t2 * t2));
		if ((t3 != 0.0f) || (t4 != 0.0f))
			f /= sqrtf((t3 * t3) + (t4 * t4));

		/* Compute first vertex color */
		l.c1.r = triangle->c3.r + f * (triangle->c1.r - triangle->c3.r);
		l.c1.g = triangle->c3.g + f * (triangle->c1.g - triangle->c3.g);
		l.c1.b = triangle->c3.b + f * (triangle->c1.b - triangle->c3.b);

		/* Compute second vertex texture U coordinate */
		l.u1 = triangle->u3 + f * (triangle->u1 - triangle->u3);

		/* Compute second line interpolation factor */
		t1 = triangle->x3 - x2;
		t2 = triangle->y3 - y;
		t3 = triangle->x3 - triangle->x2;
		t4 = triangle->y3 - triangle->y2;
		f = sqrtf((t1 * t1) + (t2 * t2));
		if ((t3 != 0.0f) || (t4 != 0.0f))
			f /= sqrtf((t3 * t3) + (t4 * t4));

		/* Compute second vertex color */
		l.c2.r = triangle->c3.r + f * (triangle->c2.r - triangle->c3.r);
		l.c2.g = triangle->c3.g + f * (triangle->c2.g - triangle->c3.g);
		l.c2.b = triangle->c3.b + f * (triangle->c2.b - triangle->c3.b);

		/* Compute second vertex texture U coordinate */
		l.u2 = triangle->u3 + f * (triangle->u2 - triangle->u3);

		/* Compute line texture V coordinate */
		f = y - triangle->y1;
		if (triangle->y1 != triangle->y3)
			f /= triangle->y3 - triangle->y1;
		l.v = triangle->v1 + f * (triangle->v3 - triangle->v1);

		/* Update X coordinates */
		x1 -= x1_inc;
		x2 -= x2_inc;

		/* Draw single line */
		draw_line(gpu, &l);
	}
}

void draw_triangle(struct gpu *gpu, struct triangle *triangle)
{
	float x;
	float y;
	struct color c;
	float u;
	float v;
	struct triangle flat;
	float f;

	/* Swap first and second vertices if needed */
	if (triangle->y2 < triangle->y1) {
		x = triangle->x1;
		y = triangle->y1;
		c = triangle->c1;
		u = triangle->u1;
		v = triangle->v1;
		triangle->x1 = triangle->x2;
		triangle->y1 = triangle->y2;
		triangle->c1 = triangle->c2;
		triangle->u1 = triangle->u2;
		triangle->v1 = triangle->v2;
		triangle->x2 = x;
		triangle->y2 = y;
		triangle->c2 = c;
		triangle->u2 = u;
		triangle->v2 = v;
	}

	/* Swap first and third vertices if needed */
	if (triangle->y3 < triangle->y1) {
		x = triangle->x1;
		y = triangle->y1;
		c = triangle->c1;
		u = triangle->u1;
		v = triangle->v1;
		triangle->x1 = triangle->x3;
		triangle->y1 = triangle->y3;
		triangle->c1 = triangle->c3;
		triangle->u1 = triangle->u3;
		triangle->v1 = triangle->v3;
		triangle->x3 = x;
		triangle->y3 = y;
		triangle->c3 = c;
		triangle->u3 = u;
		triangle->v3 = v;
	}

	/* Swap second and third vertices if needed */
	if (triangle->y3 < triangle->y2) {
		x = triangle->x2;
		y = triangle->y2;
		c = triangle->c2;
		u = triangle->u2;
		v = triangle->v2;
		triangle->x2 = triangle->x3;
		triangle->y2 = triangle->y3;
		triangle->c2 = triangle->c3;
		triangle->u2 = triangle->u3;
		triangle->v2 = triangle->v3;
		triangle->x3 = x;
		triangle->y3 = y;
		triangle->c3 = c;
		triangle->u3 = u;
		triangle->v3 = v;
	}

	/* Draw flat bottom triangle and return if possible */
	if (triangle->y2 == triangle->y3) {
		draw_triangle_flat_bottom(gpu, triangle);
		return;
	}

	/* Draw flat top triangle and return if possible */
	if (triangle->y1 == triangle->y2) {
		draw_triangle_flat_top(gpu, triangle);
		return;
	}

	/* Compute new vertex */
	f = triangle->y2 - triangle->y1;
	if (triangle->y1 != triangle->y3)
		f /= triangle->y3 - triangle->y1;
	x = triangle->x1 + f * (triangle->x3 - triangle->x1);
	y = triangle->y2;
	c.r = triangle->c1.r + f * (triangle->c3.r - triangle->c1.r);
	c.g = triangle->c1.g + f * (triangle->c3.g - triangle->c1.g);
	c.b = triangle->c1.b + f * (triangle->c3.b - triangle->c1.b);
	u = triangle->u1 + f * (triangle->u3 - triangle->u1);
	v = triangle->v2;

	/* Copy render data */
	flat.render_data = triangle->render_data;

	/* Create flat bottom triangle and draw it */
	flat.x1 = triangle->x1;
	flat.y1 = triangle->y1;
	flat.c1 = triangle->c1;
	flat.u1 = triangle->u1;
	flat.v1 = triangle->v1;
	flat.x2 = triangle->x2;
	flat.y2 = triangle->y2;
	flat.c2 = triangle->c2;
	flat.u2 = triangle->u2;
	flat.v2 = triangle->v2;
	flat.x3 = x;
	flat.y3 = y;
	flat.c3 = c;
	flat.u3 = u;
	flat.v3 = v;
	draw_triangle_flat_bottom(gpu, &flat);

	/* Create flat top triangle and draw it */
	flat.x1 = triangle->x2;
	flat.y1 = triangle->y2;
	flat.c1 = triangle->c2;
	flat.u1 = triangle->u2;
	flat.v1 = triangle->v2;
	flat.x2 = x;
	flat.y2 = y;
	flat.c2 = c;
	flat.u2 = u;
	flat.v2 = v;
	flat.x3 = triangle->x3;
	flat.y3 = triangle->y3;
	flat.c3 = triangle->c3;
	flat.u3 = triangle->u3;
	flat.v3 = triangle->v3;
	draw_triangle_flat_top(gpu, &flat);
}

void draw_rectangle(struct gpu *gpu, struct rectangle *rectangle)
{
	struct pixel pixel;
	int32_t x;
	int32_t y;
	uint8_t u;
	uint8_t v;

	/* Copy render data */
	pixel.render_data = rectangle->render_data;

	/* Set texture U coordinate LSB on horizontal flip */
	if (gpu->x_flip)
		rectangle->u |= 0x01;

	/* Initialize data */
	v = rectangle->v;

	/* Draw rectangle */
	for (y = 0; y < rectangle->height; y++) {
		/* Reset texture U coordinate */
		u = rectangle->u;

		/* Draw line */
		for (x = 0; x < rectangle->width; x++) {
			/* Build and draw pixel */
			pixel.x = rectangle->x + x;
			pixel.y = rectangle->y + y;
			pixel.c = rectangle->c;
			pixel.u = u;
			pixel.v = v;
			draw_pixel(gpu, &pixel);

			/* Increment U coordinate based on horizontal flip */
			u += gpu->x_flip ? -1 : 1;
		}

		/* Update V coordinate based on vertical flip */
		v += gpu->y_flip ? -1 : 1;
	}
}

void draw_ray(struct gpu *gpu, struct ray *ray)
{
	struct pixel pixel;
	struct color c;
	uint64_t x;
	uint64_t y;
	uint32_t r;
	uint32_t g;
	uint32_t b;
	int32_t i_dx;
	int32_t i_dy;
	int32_t dk;
	int64_t delta;
	int64_t dx_dk = 0;
	int64_t dy_dk = 0;
	int32_t dr_dk = 0;
	int32_t dg_dk = 0;
	int32_t db_dk = 0;
	int32_t i;

	/* Initialize pixel data */
	pixel.u = 0;
	pixel.v = 0;

	/* Copy render data */
	pixel.render_data = ray->render_data;

	/* Compute X/Y and main deltas */
	i_dx = abs(ray->x2 - ray->x1);
	i_dy = abs(ray->y2 - ray->y1);
	dk = (i_dx > i_dy) ? i_dx : i_dy;

	/* Discard ray if deltas are too large */
	if ((i_dx >= FB_W) || (i_dy >= FB_H))
		return;

	/* Swap points if needed */
	if ((ray->x1 >= ray->x2) && (dk != 0)) {
		x = ray->x1;
		y = ray->y1;
		c = ray->c1;
		ray->x1 = ray->x2;
		ray->y1 = ray->y2;
		ray->c1 = ray->c2;
		ray->x2 = x;
		ray->y2 = y;
		ray->c2 = c;
	}

	/* Compute steps if needed */
	if (dk != 0) {
		/* Compute X step */
		delta = (uint64_t)(ray->x2 - ray->x1) << 32;
		if (delta < 0)
			delta -= dk - 1;
		else if (delta > 0)
			delta += dk - 1;
		dx_dk = delta / dk;

		/* Compute Y step */
		delta = (uint64_t)(ray->y2 - ray->y1) << 32;
		if (delta < 0)
			delta -= dk - 1;
		else if (delta > 0)
			delta += dk - 1;
		dy_dk = delta / dk;

		/* Compute color steps */
		dr_dk = (int32_t)((uint32_t)(ray->c2.r - ray->c1.r) << 12) / dk;
		dg_dk = (int32_t)((uint32_t)(ray->c2.g - ray->c1.g) << 12) / dk;
		db_dk = (int32_t)((uint32_t)(ray->c2.b - ray->c1.b) << 12) / dk;
	}

	/* Add 32-bit decimal portion to X/Y (using fixed-point arithmetic) */
	x = ((uint64_t)ray->x1 << 32) | (1ULL << 31);
	y = ((uint64_t)ray->y1 << 32) | (1ULL << 31);

	/* Fix up coordinates */
	x -= 1024;
	if (dy_dk < 0)
		y -= 1024;

	/* Add 12-bit decimal portion to color */
	r = (ray->c1.r << 12) | (1 << 11);
	g = (ray->c1.g << 12) | (1 << 11);
	b = (ray->c1.b << 12) | (1 << 11);

	/* Draw line */
	for (i = 0; i <= dk; i++) {
		/* Build and draw pixel */
		pixel.x = (x >> 32) & 0x7FF;
		pixel.y = (y >> 32) & 0x7FF;
		pixel.c.r = (r >> 12);
		pixel.c.g = (g >> 12);
		pixel.c.b = (b >> 12);
		draw_pixel(gpu, &pixel);

		/* Update coordinates/color */
		x += dx_dk;
		y += dy_dk;
		r += dr_dk;
		g += dg_dk;
		b += db_dk;
	}
}

/* GP0(00h) - NOP
   GP0(04h..1Eh, E0h, E7h..EFh) - Mirrors of GP0(00h) - NOP */
void cmd_nop()
{
}

/* GP0(01h) - Clear Cache */
void cmd_clear_cache(struct gpu *gpu)
{
	/* Reset command buffer */
	memset(&gpu->fifo, 0, sizeof(struct fifo));

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(02h) - Fill Rectangle in VRAM */
void cmd_fill_rectangle(struct gpu *gpu)
{
	union cmd_fill_rectangle cmd;
	uint32_t offset;
	uint16_t data;
	uint16_t dest_x;
	uint16_t dest_y;
	uint16_t w;
	uint16_t h;
	uint16_t x;
	uint16_t y;

	/* Dequeue FIFO (fill rectangle needs 3 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 3))
		return;

	/* Get parameters from command, handling masking/rounding as such:
	Xpos = (Xpos AND 3F0h)                       range 0..3F0h (10h steps)
	Ypos = (Ypos AND 1FFh)                       range 0..1FFh
	Xsiz = ((Xsiz AND 3FFh) + 0Fh) AND (NOT 0Fh) range 0..400h (10h steps)
	Ysiz = (Ysiz AND 1FFh)                       range 0..1FFh */
	dest_x = cmd.top_left.x_coord & 0x3F0;
	dest_y = cmd.top_left.y_coord & 0x1FF;
	w = ((cmd.dimensions.width & 0x3FF) + 0x0F) & ~0x0F;
	h = cmd.dimensions.height & 0x1FF;

	/* Fill color components (discarding lower 3 bits of each color) */
	data = 0;
	bitops_setw(&data, 0, 5, cmd.color.red >> 3);
	bitops_setw(&data, 5, 5, cmd.color.green >> 3);
	bitops_setw(&data, 10, 5, cmd.color.blue >> 3);

	/* Fill rectangle (fill is not affected by mask settings) */
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			/* Compute offset */
			offset = (dest_x + x) % FB_W;
			offset += ((dest_y + y) % FB_H) * FB_W;
			offset *= sizeof(uint16_t);

			/* Write data to frame buffer */
			gpu->vram[offset] = data >> 8;
			gpu->vram[offset + 1] = data;
		}

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(20h) - Monochrome three-point polygon, opaque
   GP0(22h) - Monochrome three-point polygon, semi-transparent */
void cmd_monochrome_3p_poly(struct gpu *gpu, bool opaque)
{
	union cmd_monochrome_poly cmd;
	struct render_data render_data;
	struct triangle triangle;
	struct color c;

	/* For polygon and line rendering commands, the receive DMA block
	ready bit gets cleared immediately after receiving the command word. */
	gpu->stat.ready_recv_dma = 0;

	/* Dequeue FIFO (monochrome 3-point polygon needs 4 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 4))
		return;

	/* Extract color from command */
	c.r = cmd.color.red;
	c.g = cmd.color.green;
	c.b = cmd.color.blue;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = false;
	render_data.dithering = false;
	triangle.render_data = &render_data;

	/* Build and draw triangle */
	triangle.x1 = cmd.v1.x_coord;
	triangle.y1 = cmd.v1.y_coord;
	triangle.c1 = c;
	triangle.x2 = cmd.v2.x_coord;
	triangle.y2 = cmd.v2.y_coord;
	triangle.c2 = c;
	triangle.x3 = cmd.v3.x_coord;
	triangle.y3 = cmd.v3.y_coord;
	triangle.c3 = c;
	draw_triangle(gpu, &triangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(28h) - Monochrome four-point polygon, opaque
   GP0(2Ah) - Monochrome four-point polygon, semi-transparent */
void cmd_monochrome_4p_poly(struct gpu *gpu, bool opaque)
{
	union cmd_monochrome_poly cmd;
	struct render_data render_data;
	struct triangle triangle;
	struct color c;

	/* For polygon and line rendering commands, the receive DMA block
	ready bit gets cleared immediately after receiving the command word. */
	gpu->stat.ready_recv_dma = 0;

	/* Dequeue FIFO (monochrome 4-point polygon needs 5 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 5))
		return;

	/* Extract color from command */
	c.r = cmd.color.red;
	c.g = cmd.color.green;
	c.b = cmd.color.blue;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = false;
	render_data.dithering = false;
	triangle.render_data = &render_data;

	/* Build and draw first triangle */
	triangle.x1 = cmd.v1.x_coord;
	triangle.y1 = cmd.v1.y_coord;
	triangle.c1 = c;
	triangle.x2 = cmd.v2.x_coord;
	triangle.y2 = cmd.v2.y_coord;
	triangle.c2 = c;
	triangle.x3 = cmd.v3.x_coord;
	triangle.y3 = cmd.v3.y_coord;
	triangle.c3 = c;
	draw_triangle(gpu, &triangle);

	/* Build and draw second triangle */
	triangle.x1 = cmd.v2.x_coord;
	triangle.y1 = cmd.v2.y_coord;
	triangle.c1 = c;
	triangle.x2 = cmd.v3.x_coord;
	triangle.y2 = cmd.v3.y_coord;
	triangle.c2 = c;
	triangle.x3 = cmd.v4.x_coord;
	triangle.y3 = cmd.v4.y_coord;
	triangle.c3 = c;
	draw_triangle(gpu, &triangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(24h) - Textured three-point polygon, opaque, texture-blending
   GP0(25h) - Textured three-point polygon, opaque, raw-texture
   GP0(26h) - Textured three-point polygon, semi-transparent, texture-blending
   GP0(27h) - Textured three-point polygon, semi-transparent, raw-texture */
void cmd_textured_3p_poly(struct gpu *gpu, bool opaque, bool raw)
{
	union cmd_textured_poly cmd;
	struct render_data render_data;
	struct triangle triangle;
	struct color c;

	/* For polygon and line rendering commands, the receive DMA block
	ready bit gets cleared immediately after receiving the command word. */
	gpu->stat.ready_recv_dma = 0;

	/* Dequeue FIFO (textured 3-point polygon needs 7 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 7))
		return;

	/* Extract color from command */
	c.r = cmd.color.red;
	c.g = cmd.color.green;
	c.b = cmd.color.blue;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = cmd.tex_page.semi_transparency;
	render_data.textured = true;
	render_data.raw = raw;
	render_data.dithering = (gpu->stat.dither && !raw);
	render_data.tex_page_x = cmd.tex_page.x_base;
	render_data.tex_page_y = cmd.tex_page.y_base;
	render_data.tex_page_colors = cmd.tex_page.colors;
	render_data.clut_x = cmd.palette.x_coord;
	render_data.clut_y = cmd.palette.y_coord;
	triangle.render_data = &render_data;

	/* Build and draw triangle */
	triangle.x1 = cmd.v1.x_coord;
	triangle.y1 = cmd.v1.y_coord;
	triangle.c1 = c;
	triangle.u1 = cmd.tex_coords1.u;
	triangle.v1 = cmd.tex_coords1.v;
	triangle.x2 = cmd.v2.x_coord;
	triangle.y2 = cmd.v2.y_coord;
	triangle.c2 = c;
	triangle.u2 = cmd.tex_coords2.u;
	triangle.v2 = cmd.tex_coords2.v;
	triangle.x3 = cmd.v3.x_coord;
	triangle.y3 = cmd.v3.y_coord;
	triangle.c3 = c;
	triangle.u3 = cmd.tex_coords3.u;
	triangle.v3 = cmd.tex_coords3.v;
	draw_triangle(gpu, &triangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(2Ch) - Textured four-point polygon, opaque, texture-blending
   GP0(2Dh) - Textured four-point polygon, opaque, raw-texture
   GP0(2Eh) - Textured four-point polygon, semi-transparent, texture-blending
   GP0(2Fh) - Textured four-point polygon, semi-transparent, raw-texture */
void cmd_textured_4p_poly(struct gpu *gpu, bool opaque, bool raw)
{
	union cmd_textured_poly cmd;
	struct render_data render_data;
	struct triangle triangle;
	struct color c;

	/* For polygon and line rendering commands, the receive DMA block
	ready bit gets cleared immediately after receiving the command word. */
	gpu->stat.ready_recv_dma = 0;

	/* Dequeue FIFO (textured 4-point polygon needs 9 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 9))
		return;

	/* Extract color from command */
	c.r = cmd.color.red;
	c.g = cmd.color.green;
	c.b = cmd.color.blue;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = cmd.tex_page.semi_transparency;
	render_data.textured = true;
	render_data.raw = raw;
	render_data.dithering = (gpu->stat.dither && !raw);
	render_data.tex_page_x = cmd.tex_page.x_base;
	render_data.tex_page_y = cmd.tex_page.y_base;
	render_data.tex_page_colors = cmd.tex_page.colors;
	render_data.clut_x = cmd.palette.x_coord;
	render_data.clut_y = cmd.palette.y_coord;
	triangle.render_data = &render_data;

	/* Build and draw first triangle */
	triangle.x1 = cmd.v1.x_coord;
	triangle.y1 = cmd.v1.y_coord;
	triangle.c1 = c;
	triangle.u1 = cmd.tex_coords1.u;
	triangle.v1 = cmd.tex_coords1.v;
	triangle.x2 = cmd.v2.x_coord;
	triangle.y2 = cmd.v2.y_coord;
	triangle.c2 = c;
	triangle.u2 = cmd.tex_coords2.u;
	triangle.v2 = cmd.tex_coords2.v;
	triangle.x3 = cmd.v3.x_coord;
	triangle.y3 = cmd.v3.y_coord;
	triangle.c3 = c;
	triangle.u3 = cmd.tex_coords3.u;
	triangle.v3 = cmd.tex_coords3.v;
	draw_triangle(gpu, &triangle);

	/* Build and draw second triangle */
	triangle.x1 = cmd.v2.x_coord;
	triangle.y1 = cmd.v2.y_coord;
	triangle.c1 = c;
	triangle.u1 = cmd.tex_coords2.u;
	triangle.v1 = cmd.tex_coords2.v;
	triangle.x2 = cmd.v3.x_coord;
	triangle.y2 = cmd.v3.y_coord;
	triangle.c2 = c;
	triangle.u2 = cmd.tex_coords3.u;
	triangle.v2 = cmd.tex_coords3.v;
	triangle.x3 = cmd.v4.x_coord;
	triangle.y3 = cmd.v4.y_coord;
	triangle.c3 = c;
	triangle.u3 = cmd.tex_coords4.u;
	triangle.v3 = cmd.tex_coords4.v;
	draw_triangle(gpu, &triangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(30h) - Shaded three-point polygon, opaque
   GP0(32h) - Shaded three-point polygon, semi-transparent */
void cmd_shaded_3p_poly(struct gpu *gpu, bool opaque)
{
	union cmd_shaded_poly cmd;
	struct render_data render_data;
	struct triangle triangle;

	/* For polygon and line rendering commands, the receive DMA block
	ready bit gets cleared immediately after receiving the command word. */
	gpu->stat.ready_recv_dma = 0;

	/* Dequeue FIFO (shaded 3-point polygon needs 6 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 6))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = false;
	render_data.dithering = gpu->stat.dither;
	triangle.render_data = &render_data;

	/* Build and draw triangle */
	triangle.x1 = cmd.v1.x_coord;
	triangle.y1 = cmd.v1.y_coord;
	triangle.c1.r = cmd.color1.red;
	triangle.c1.g = cmd.color1.green;
	triangle.c1.b = cmd.color1.blue;
	triangle.x2 = cmd.v2.x_coord;
	triangle.y2 = cmd.v2.y_coord;
	triangle.c2.r = cmd.color2.red;
	triangle.c2.g = cmd.color2.green;
	triangle.c2.b = cmd.color2.blue;
	triangle.x3 = cmd.v3.x_coord;
	triangle.y3 = cmd.v3.y_coord;
	triangle.c3.r = cmd.color3.red;
	triangle.c3.g = cmd.color3.green;
	triangle.c3.b = cmd.color3.blue;
	draw_triangle(gpu, &triangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(38h) - Shaded four-point polygon, opaque
   GP0(3Ah) - Shaded four-point polygon, semi-transparent */
void cmd_shaded_4p_poly(struct gpu *gpu, bool opaque)
{
	union cmd_shaded_poly cmd;
	struct render_data render_data;
	struct triangle triangle;

	/* For polygon and line rendering commands, the receive DMA block
	ready bit gets cleared immediately after receiving the command word. */
	gpu->stat.ready_recv_dma = 0;

	/* Dequeue FIFO (shaded 4-point polygon needs 8 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 8))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = false;
	render_data.dithering = gpu->stat.dither;
	triangle.render_data = &render_data;

	/* Build and draw first triangle */
	triangle.x1 = cmd.v1.x_coord;
	triangle.y1 = cmd.v1.y_coord;
	triangle.c1.r = cmd.color1.red;
	triangle.c1.g = cmd.color1.green;
	triangle.c1.b = cmd.color1.blue;
	triangle.x2 = cmd.v2.x_coord;
	triangle.y2 = cmd.v2.y_coord;
	triangle.c2.r = cmd.color2.red;
	triangle.c2.g = cmd.color2.green;
	triangle.c2.b = cmd.color2.blue;
	triangle.x3 = cmd.v3.x_coord;
	triangle.y3 = cmd.v3.y_coord;
	triangle.c3.r = cmd.color3.red;
	triangle.c3.g = cmd.color3.green;
	triangle.c3.b = cmd.color3.blue;
	draw_triangle(gpu, &triangle);

	/* Build and draw second triangle */
	triangle.x1 = cmd.v2.x_coord;
	triangle.y1 = cmd.v2.y_coord;
	triangle.c1.r = cmd.color2.red;
	triangle.c1.g = cmd.color2.green;
	triangle.c1.b = cmd.color2.blue;
	triangle.x2 = cmd.v3.x_coord;
	triangle.y2 = cmd.v3.y_coord;
	triangle.c2.r = cmd.color3.red;
	triangle.c2.g = cmd.color3.green;
	triangle.c2.b = cmd.color3.blue;
	triangle.x3 = cmd.v4.x_coord;
	triangle.y3 = cmd.v4.y_coord;
	triangle.c3.r = cmd.color4.red;
	triangle.c3.g = cmd.color4.green;
	triangle.c3.b = cmd.color4.blue;
	draw_triangle(gpu, &triangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(34h) - Shaded Textured three-point polygon, opaque, blending
   GP0(36h) - Shaded Textured three-point polygon, semi-transparent, blending */
void cmd_shaded_textured_3p_poly(struct gpu *gpu, bool opaque)
{
	union cmd_shaded_textured_poly cmd;
	struct render_data render_data;
	struct triangle triangle;

	/* For polygon and line rendering commands, the receive DMA block
	ready bit gets cleared immediately after receiving the command word. */
	gpu->stat.ready_recv_dma = 0;

	/* Dequeue FIFO (textured 3-point polygon needs 9 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 9))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = cmd.tex_page.semi_transparency;
	render_data.textured = true;
	render_data.raw = false;
	render_data.dithering = gpu->stat.dither;
	render_data.tex_page_x = cmd.tex_page.x_base;
	render_data.tex_page_y = cmd.tex_page.y_base;
	render_data.tex_page_colors = cmd.tex_page.colors;
	render_data.clut_x = cmd.palette.x_coord;
	render_data.clut_y = cmd.palette.y_coord;
	triangle.render_data = &render_data;

	/* Build and draw first triangle */
	triangle.x1 = cmd.v1.x_coord;
	triangle.y1 = cmd.v1.y_coord;
	triangle.c1.r = cmd.color1.red;
	triangle.c1.g = cmd.color1.green;
	triangle.c1.b = cmd.color1.blue;
	triangle.u1 = cmd.tex_coords1.u;
	triangle.v1 = cmd.tex_coords1.v;
	triangle.x2 = cmd.v2.x_coord;
	triangle.y2 = cmd.v2.y_coord;
	triangle.c2.r = cmd.color2.red;
	triangle.c2.g = cmd.color2.green;
	triangle.c2.b = cmd.color2.blue;
	triangle.u2 = cmd.tex_coords2.u;
	triangle.v2 = cmd.tex_coords2.v;
	triangle.x3 = cmd.v3.x_coord;
	triangle.y3 = cmd.v3.y_coord;
	triangle.c3.r = cmd.color3.red;
	triangle.c3.g = cmd.color3.green;
	triangle.c3.b = cmd.color3.blue;
	triangle.u3 = cmd.tex_coords3.u;
	triangle.v3 = cmd.tex_coords3.v;
	draw_triangle(gpu, &triangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(3Ch) - Shaded Textured four-point polygon, opaque, blending
   GP0(3Eh) - Shaded Textured four-point polygon, semi-transparent, blending */
void cmd_shaded_textured_4p_poly(struct gpu *gpu, bool opaque)
{
	union cmd_shaded_textured_poly cmd;
	struct render_data render_data;
	struct triangle triangle;

	/* For polygon and line rendering commands, the receive DMA block
	ready bit gets cleared immediately after receiving the command word. */
	gpu->stat.ready_recv_dma = 0;

	/* Dequeue FIFO (textured 4-point polygon needs 12 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 12))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = cmd.tex_page.semi_transparency;
	render_data.textured = true;
	render_data.raw = false;
	render_data.dithering = gpu->stat.dither;
	render_data.tex_page_x = cmd.tex_page.x_base;
	render_data.tex_page_y = cmd.tex_page.y_base;
	render_data.tex_page_colors = cmd.tex_page.colors;
	render_data.clut_x = cmd.palette.x_coord;
	render_data.clut_y = cmd.palette.y_coord;
	triangle.render_data = &render_data;

	/* Build and draw first triangle */
	triangle.x1 = cmd.v1.x_coord;
	triangle.y1 = cmd.v1.y_coord;
	triangle.c1.r = cmd.color1.red;
	triangle.c1.g = cmd.color1.green;
	triangle.c1.b = cmd.color1.blue;
	triangle.u1 = cmd.tex_coords1.u;
	triangle.v1 = cmd.tex_coords1.v;
	triangle.x2 = cmd.v2.x_coord;
	triangle.y2 = cmd.v2.y_coord;
	triangle.c2.r = cmd.color2.red;
	triangle.c2.g = cmd.color2.green;
	triangle.c2.b = cmd.color2.blue;
	triangle.u2 = cmd.tex_coords2.u;
	triangle.v2 = cmd.tex_coords2.v;
	triangle.x3 = cmd.v3.x_coord;
	triangle.y3 = cmd.v3.y_coord;
	triangle.c3.r = cmd.color3.red;
	triangle.c3.g = cmd.color3.green;
	triangle.c3.b = cmd.color3.blue;
	triangle.u3 = cmd.tex_coords3.u;
	triangle.v3 = cmd.tex_coords3.v;
	draw_triangle(gpu, &triangle);

	/* Build and draw second triangle */
	triangle.x1 = cmd.v2.x_coord;
	triangle.y1 = cmd.v2.y_coord;
	triangle.c1.r = cmd.color2.red;
	triangle.c1.g = cmd.color2.green;
	triangle.c1.b = cmd.color2.blue;
	triangle.u1 = cmd.tex_coords2.u;
	triangle.v1 = cmd.tex_coords2.v;
	triangle.x2 = cmd.v3.x_coord;
	triangle.y2 = cmd.v3.y_coord;
	triangle.c2.r = cmd.color3.red;
	triangle.c2.g = cmd.color3.green;
	triangle.c2.b = cmd.color3.blue;
	triangle.u2 = cmd.tex_coords3.u;
	triangle.v2 = cmd.tex_coords3.v;
	triangle.x3 = cmd.v4.x_coord;
	triangle.y3 = cmd.v4.y_coord;
	triangle.c3.r = cmd.color4.red;
	triangle.c3.g = cmd.color4.green;
	triangle.c3.b = cmd.color4.blue;
	triangle.u3 = cmd.tex_coords4.u;
	triangle.v3 = cmd.tex_coords4.v;
	draw_triangle(gpu, &triangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(40h) - Monochrome line, opaque
   GP0(42h) - Monochrome line, semi-transparent */
void cmd_monochrome_line(struct gpu *gpu, bool opaque)
{
	union cmd_monochrome_line cmd;
	struct render_data render_data;
	struct ray ray;

	/* For polygon and line rendering commands, the receive DMA block
	ready bit gets cleared immediately after receiving the command word. */
	gpu->stat.ready_recv_dma = 0;

	/* Dequeue FIFO (monochrome line needs 3 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 3))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = false;
	render_data.dithering = gpu->stat.dither;
	ray.render_data = &render_data;

	/* Build and draw ray */
	ray.x1 = cmd.v1.x_coord;
	ray.y1 = cmd.v1.y_coord;
	ray.c1.r = cmd.color.red;
	ray.c1.g = cmd.color.green;
	ray.c1.b = cmd.color.blue;
	ray.x2 = cmd.v2.x_coord;
	ray.y2 = cmd.v2.y_coord;
	ray.c2.r = cmd.color.red;
	ray.c2.g = cmd.color.green;
	ray.c2.b = cmd.color.blue;
	draw_ray(gpu, &ray);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(48h) - Monochrome Poly-line, opaque
   GP0(4Ah) - Monochrome Poly-line, semi-transparent */
void cmd_monochrome_poly_line(struct gpu *gpu, bool opaque)
{
	union cmd_monochrome_line cmd;
	struct render_data render_data;
	union vertex v;
	struct ray ray;

	/* For polygon and line rendering commands, the receive DMA block
	ready bit gets cleared immediately after receiving the command word. */
	gpu->stat.ready_recv_dma = 0;

	/* Handle appropriate poly-line step */
	switch (gpu->poly_line_data.step) {
	case 0:
		/* Dequeue FIFO (monochrome poly-line needs 3 arguments) */
		if (!fifo_dequeue(&gpu->fifo, cmd.raw, 3))
			return;

		/* Set attributes */
		gpu->poly_line_data.v1 = cmd.v1;
		gpu->poly_line_data.v2 = cmd.v2;
		gpu->poly_line_data.color1 = cmd.color;
		gpu->poly_line_data.color2 = cmd.color;

		/* Go to next step */
		gpu->poly_line_data.step++;
		break;
	case 1:
		/* Dequeue FIFO (1 vertex) */
		if (!fifo_dequeue(&gpu->fifo, &v.raw, 1))
			return;

		/* Set attributes */
		gpu->poly_line_data.v1 = gpu->poly_line_data.v2;
		gpu->poly_line_data.v2 = v;
		break;
	}

	/* Check for termination: the termination code should be usually
	55555555h but could be 50005000h (unknown which exact bits/values are
	relevant there). */
	if ((gpu->poly_line_data.v2.raw & 0xF000F000) == 0x50005000) {
		gpu->fifo.cmd_in_progress = false;
		return;
	}

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = false;
	render_data.dithering = gpu->stat.dither;
	ray.render_data = &render_data;

	/* Build and draw ray */
	ray.x1 = gpu->poly_line_data.v1.x_coord;
	ray.y1 = gpu->poly_line_data.v1.y_coord;
	ray.c1.r = gpu->poly_line_data.color1.red;
	ray.c1.g = gpu->poly_line_data.color1.green;
	ray.c1.b = gpu->poly_line_data.color1.blue;
	ray.x2 = gpu->poly_line_data.v2.x_coord;
	ray.y2 = gpu->poly_line_data.v2.y_coord;
	ray.c2.r = gpu->poly_line_data.color2.red;
	ray.c2.g = gpu->poly_line_data.color2.green;
	ray.c2.b = gpu->poly_line_data.color2.blue;
	draw_ray(gpu, &ray);
}

/* GP0(50h) - Shaded line, opaque
   GP0(52h) - Shaded line, semi-transparent */
void cmd_shaded_line(struct gpu *gpu, bool opaque)
{
	union cmd_shaded_line cmd;
	struct render_data render_data;
	struct ray ray;

	/* For polygon and line rendering commands, the receive DMA block
	ready bit gets cleared immediately after receiving the command word. */
	gpu->stat.ready_recv_dma = 0;

	/* Dequeue FIFO (shaded line needs 4 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 4))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = false;
	render_data.dithering = gpu->stat.dither;
	ray.render_data = &render_data;

	/* Build and draw ray */
	ray.x1 = cmd.v1.x_coord;
	ray.y1 = cmd.v1.y_coord;
	ray.c1.r = cmd.color1.red;
	ray.c1.g = cmd.color1.green;
	ray.c1.b = cmd.color1.blue;
	ray.x2 = cmd.v2.x_coord;
	ray.y2 = cmd.v2.y_coord;
	ray.c2.r = cmd.color2.red;
	ray.c2.g = cmd.color2.green;
	ray.c2.b = cmd.color2.blue;
	draw_ray(gpu, &ray);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(58h) - Shaded Poly-line, opaque
   GP0(5Ah) - Shaded Poly-line, semi-transparent */
void cmd_shaded_poly_line(struct gpu *gpu, bool opaque)
{
	union cmd_shaded_line cmd;
	struct render_data render_data;
	union vertex v;
	union color_attr color;
	struct ray ray;
	bool render;

	/* For polygon and line rendering commands, the receive DMA block
	ready bit gets cleared immediately after receiving the command word. */
	gpu->stat.ready_recv_dma = 0;

	/* Handle appropriate poly-line step */
	render = true;
	switch (gpu->poly_line_data.step) {
	case 0:
		/* Dequeue FIFO (shaded poly-line needs 4 arguments) */
		if (!fifo_dequeue(&gpu->fifo, cmd.raw, 4))
			return;

		/* Set attributes */
		gpu->poly_line_data.v1 = cmd.v1;
		gpu->poly_line_data.v2 = cmd.v2;
		gpu->poly_line_data.color1 = cmd.color1;
		gpu->poly_line_data.color2 = cmd.color2;

		/* Go to next step */
		gpu->poly_line_data.step++;
		break;
	case 1:
		/* Dequeue FIFO (1 color) */
		if (!fifo_dequeue(&gpu->fifo, &color.raw, 1))
			return;

		/* Set attributes */
		gpu->poly_line_data.color1 = gpu->poly_line_data.color2;
		gpu->poly_line_data.color2 = color;

		/* We are not ready to render yet (waiting for vertex) */
		render = false;

		/* Go to next step */
		gpu->poly_line_data.step++;
		break;
	case 2:
		/* Dequeue FIFO (1 vertex) */
		if (!fifo_dequeue(&gpu->fifo, &v.raw, 1))
			return;

		/* Set attributes */
		gpu->poly_line_data.v1 = gpu->poly_line_data.v2;
		gpu->poly_line_data.v2 = v;

		/* Go back to previous step */
		gpu->poly_line_data.step--;
		break;
	}

	/* Check for termination: the termination code should be usually
	55555555h but could be 50005000h (unknown which exact bits/values are
	relevant there). */
	if ((gpu->poly_line_data.color2.raw & 0xF000F000) == 0x50005000) {
		gpu->fifo.cmd_in_progress = false;
		return;
	}

	/* Return already if set of attributes is incomplete */
	if (!render)
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = false;
	render_data.dithering = gpu->stat.dither;
	ray.render_data = &render_data;

	/* Build and draw ray */
	ray.x1 = gpu->poly_line_data.v1.x_coord;
	ray.y1 = gpu->poly_line_data.v1.y_coord;
	ray.c1.r = gpu->poly_line_data.color1.red;
	ray.c1.g = gpu->poly_line_data.color1.green;
	ray.c1.b = gpu->poly_line_data.color1.blue;
	ray.x2 = gpu->poly_line_data.v2.x_coord;
	ray.y2 = gpu->poly_line_data.v2.y_coord;
	ray.c2.r = gpu->poly_line_data.color2.red;
	ray.c2.g = gpu->poly_line_data.color2.green;
	ray.c2.b = gpu->poly_line_data.color2.blue;
	draw_ray(gpu, &ray);
}

/* GP0(60h) - Monochrome Rectangle (variable size) (opaque)
   GP0(62h) - Monochrome Rectangle (variable size) (semi-transparent) */
void cmd_monochrome_rect_var(struct gpu *gpu, bool opaque)
{
	union cmd_monochrome_rect cmd;
	struct render_data render_data;
	struct rectangle rectangle;

	/* Dequeue FIFO (variable size monochrome rect needs 3 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 3))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = false;
	render_data.dithering = false;
	rectangle.render_data = &render_data;

	/* Build and draw rectangle */
	rectangle.x = cmd.top_left.x_coord;
	rectangle.y = cmd.top_left.y_coord;
	rectangle.width = cmd.dimensions.width;
	rectangle.height = cmd.dimensions.height;
	rectangle.c.r = cmd.color.red;
	rectangle.c.g = cmd.color.green;
	rectangle.c.b = cmd.color.blue;
	rectangle.u = 0;
	rectangle.v = 0;
	draw_rectangle(gpu, &rectangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(68h) - Monochrome Rectangle (1x1) (Dot) (opaque)
   GP0(6Ah) - Monochrome Rectangle (1x1) (Dot) (semi-transparent) */
void cmd_monochrome_rect_1x1(struct gpu *gpu, bool opaque)
{
	union cmd_monochrome_rect cmd;
	struct render_data render_data;
	struct rectangle rectangle;

	/* Dequeue FIFO (1x1 monochrome rect needs 2 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 2))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = false;
	render_data.dithering = false;
	rectangle.render_data = &render_data;

	/* Build and draw rectangle */
	rectangle.x = cmd.top_left.x_coord;
	rectangle.y = cmd.top_left.y_coord;
	rectangle.width = 1;
	rectangle.height = 1;
	rectangle.c.r = cmd.color.red;
	rectangle.c.g = cmd.color.green;
	rectangle.c.b = cmd.color.blue;
	rectangle.u = 0;
	rectangle.v = 0;
	draw_rectangle(gpu, &rectangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(70h) - Monochrome Rectangle (8x8) (opaque)
   GP0(72h) - Monochrome Rectangle (8x8) (semi-transparent) */
void cmd_monochrome_rect_8x8(struct gpu *gpu, bool opaque)
{
	union cmd_monochrome_rect cmd;
	struct render_data render_data;
	struct rectangle rectangle;

	/* Dequeue FIFO (8x8 monochrome rect needs 2 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 2))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = false;
	render_data.dithering = false;
	rectangle.render_data = &render_data;

	/* Build and draw rectangle */
	rectangle.x = cmd.top_left.x_coord;
	rectangle.y = cmd.top_left.y_coord;
	rectangle.width = 8;
	rectangle.height = 8;
	rectangle.c.r = cmd.color.red;
	rectangle.c.g = cmd.color.green;
	rectangle.c.b = cmd.color.blue;
	rectangle.u = 0;
	rectangle.v = 0;
	draw_rectangle(gpu, &rectangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(78h) - Monochrome Rectangle (16x16) (opaque)
   GP0(7Ah) - Monochrome Rectangle (16x16) (semi-transparent) */
void cmd_monochrome_rect_16x16(struct gpu *gpu, bool opaque)
{
	union cmd_monochrome_rect cmd;
	struct render_data render_data;
	struct rectangle rectangle;

	/* Dequeue FIFO (16x16 monochrome rect needs 2 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 2))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = false;
	render_data.dithering = false;
	rectangle.render_data = &render_data;

	/* Build and draw rectangle */
	rectangle.x = cmd.top_left.x_coord;
	rectangle.y = cmd.top_left.y_coord;
	rectangle.width = 16;
	rectangle.height = 16;
	rectangle.c.r = cmd.color.red;
	rectangle.c.g = cmd.color.green;
	rectangle.c.b = cmd.color.blue;
	rectangle.u = 0;
	rectangle.v = 0;
	draw_rectangle(gpu, &rectangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(64h) - Textured Rectangle, variable size, opaque, texture-blending
   GP0(65h) - Textured Rectangle, variable size, opaque, raw-texture
   GP0(66h) - Textured Rectangle, variable size, semi-transp, texture-blending
   GP0(67h) - Textured Rectangle, variable size, semi-transp, raw-texture */
void cmd_tex_rect_var(struct gpu *gpu, bool opaque, bool raw)
{
	union cmd_textured_rect cmd;
	struct render_data render_data;
	struct rectangle rectangle;

	/* Dequeue FIFO (variable size textured rectangle needs 4 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 4))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = true;
	render_data.raw = raw;
	render_data.dithering = false;
	render_data.tex_page_x = gpu->stat.tex_page_x_base;
	render_data.tex_page_y = gpu->stat.tex_page_y_base;
	render_data.tex_page_colors = gpu->stat.tex_page_colors;
	render_data.clut_x = cmd.palette.x_coord;
	render_data.clut_y = cmd.palette.y_coord;
	rectangle.render_data = &render_data;

	/* Build and draw rectangle */
	rectangle.x = cmd.top_left.x_coord;
	rectangle.y = cmd.top_left.y_coord;
	rectangle.width = cmd.dimensions.width;
	rectangle.height = cmd.dimensions.height;
	rectangle.c.r = cmd.color.red;
	rectangle.c.g = cmd.color.green;
	rectangle.c.b = cmd.color.blue;
	rectangle.u = cmd.tex_coords.u;
	rectangle.v = cmd.tex_coords.v;
	draw_rectangle(gpu, &rectangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(6Ch) - Textured Rectangle, 1x1, opaque, texture-blending
   GP0(6Dh) - Textured Rectangle, 1x1, opaque, raw-texture
   GP0(6Eh) - Textured Rectangle, 1x1, semi-transparent, texture-blending
   GP0(6Fh) - Textured Rectangle, 1x1, semi-transparent, raw-texture */
void cmd_tex_rect_1x1(struct gpu *gpu, bool opaque, bool raw)
{
	union cmd_textured_rect cmd;
	struct render_data render_data;
	struct rectangle rectangle;

	/* Dequeue FIFO (1x1 textured rectangle needs 3 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 3))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = true;
	render_data.raw = raw;
	render_data.dithering = false;
	render_data.tex_page_x = gpu->stat.tex_page_x_base;
	render_data.tex_page_y = gpu->stat.tex_page_y_base;
	render_data.tex_page_colors = gpu->stat.tex_page_colors;
	render_data.clut_x = cmd.palette.x_coord;
	render_data.clut_y = cmd.palette.y_coord;
	rectangle.render_data = &render_data;

	/* Build and draw rectangle */
	rectangle.x = cmd.top_left.x_coord;
	rectangle.y = cmd.top_left.y_coord;
	rectangle.width = 1;
	rectangle.height = 1;
	rectangle.c.r = cmd.color.red;
	rectangle.c.g = cmd.color.green;
	rectangle.c.b = cmd.color.blue;
	rectangle.u = cmd.tex_coords.u;
	rectangle.v = cmd.tex_coords.v;
	draw_rectangle(gpu, &rectangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(74h) - Textured Rectangle, 8x8, opaque, texture-blending
   GP0(75h) - Textured Rectangle, 8x8, opaque, raw-texture
   GP0(76h) - Textured Rectangle, 8x8, semi-transparent, texture-blending
   GP0(77h) - Textured Rectangle, 8x8, semi-transparent, raw-texture */
void cmd_tex_rect_8x8(struct gpu *gpu, bool opaque, bool raw)
{
	union cmd_textured_rect cmd;
	struct render_data render_data;
	struct rectangle rectangle;

	/* Dequeue FIFO (8x8 textured rectangle needs 3 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 3))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = true;
	render_data.raw = raw;
	render_data.dithering = false;
	render_data.tex_page_x = gpu->stat.tex_page_x_base;
	render_data.tex_page_y = gpu->stat.tex_page_y_base;
	render_data.tex_page_colors = gpu->stat.tex_page_colors;
	render_data.clut_x = cmd.palette.x_coord;
	render_data.clut_y = cmd.palette.y_coord;
	rectangle.render_data = &render_data;

	/* Build and draw rectangle */
	rectangle.x = cmd.top_left.x_coord;
	rectangle.y = cmd.top_left.y_coord;
	rectangle.width = 8;
	rectangle.height = 8;
	rectangle.c.r = cmd.color.red;
	rectangle.c.g = cmd.color.green;
	rectangle.c.b = cmd.color.blue;
	rectangle.u = cmd.tex_coords.u;
	rectangle.v = cmd.tex_coords.v;
	draw_rectangle(gpu, &rectangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(7Ch) - Textured Rectangle, 16x16, opaque, texture-blending
   GP0(7Dh) - Textured Rectangle, 16x16, opaque, raw-texture
   GP0(7Eh) - Textured Rectangle, 16x16, semi-transparent, texture-blending
   GP0(7Fh) - Textured Rectangle, 16x16, semi-transparent, raw-texture */
void cmd_tex_rect_16x16(struct gpu *gpu, bool opaque, bool raw)
{
	union cmd_textured_rect cmd;
	struct render_data render_data;
	struct rectangle rectangle;

	/* Dequeue FIFO (16x16 textured rectangle needs 3 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 3))
		return;

	/* Set render data */
	render_data.opaque = opaque;
	render_data.semi_transparency = gpu->stat.semi_transparency;
	render_data.textured = true;
	render_data.raw = raw;
	render_data.dithering = false;
	render_data.tex_page_x = gpu->stat.tex_page_x_base;
	render_data.tex_page_y = gpu->stat.tex_page_y_base;
	render_data.tex_page_colors = gpu->stat.tex_page_colors;
	render_data.clut_x = cmd.palette.x_coord;
	render_data.clut_y = cmd.palette.y_coord;
	rectangle.render_data = &render_data;

	/* Build and draw rectangle */
	rectangle.x = cmd.top_left.x_coord;
	rectangle.y = cmd.top_left.y_coord;
	rectangle.width = 16;
	rectangle.height = 16;
	rectangle.c.r = cmd.color.red;
	rectangle.c.g = cmd.color.green;
	rectangle.c.b = cmd.color.blue;
	rectangle.u = cmd.tex_coords.u;
	rectangle.v = cmd.tex_coords.v;
	draw_rectangle(gpu, &rectangle);

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(80h) - Copy Rectangle (VRAM to VRAM) */
void cmd_copy_rect_vram_to_vram(struct gpu *gpu)
{
	union cmd_copy_rect_vram_to_vram cmd;
	uint32_t src_off;
	uint32_t dest_off;
	uint16_t half_word;
	uint16_t src_x;
	uint16_t src_y;
	uint16_t dest_x;
	uint16_t dest_y;
	uint16_t w;
	uint16_t h;
	int x;
	int y;

	/* Dequeue FIFO (copy rectangle VRAM to VRAM needs 4 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 4))
		return;

	/* Get parameters from command, handling masking as such:
	Xpos = (Xpos AND 3FFh)           range 0..3FFh
	Ypos = (Ypos AND 1FFh)           range 0..1FFh
	Xsiz = ((Xsiz - 1) AND 3FFh) + 1 range 1..400h
	Ysiz = ((Ysiz - 1) AND 1FFh) + 1 range 1..200h */
	src_x = cmd.src.x_coord & 0x3FF;
	src_y = cmd.src.y_coord % 0x1FF;
	dest_x = cmd.dest.x_coord & 0x3FF;
	dest_y = cmd.dest.y_coord % 0x1FF;
	w = ((cmd.dimensions.width - 1) & 0x3FF) + 1;
	h = ((cmd.dimensions.height - 1) & 0x1FF) + 1;

	/* Copy data */
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			/* Compute destination offset in frame buffer */
			dest_off = (dest_x + x) % FB_W;
			dest_off += ((dest_y + y) % FB_H) * FB_W;
			dest_off *= sizeof(uint16_t);

			/* Get existing pixel data within frame buffer */
			half_word = gpu->vram[dest_off] << 8;
			half_word |= gpu->vram[dest_off + 1];

			/* The transfer is affected by mask setting */
			if (!gpu->stat.draw_pixels ||
				!bitops_getw(&half_word, 15, 1)) {
				/* Compute source offset in frame buffer */
				src_off = (src_x + x) % FB_W;
				src_off += ((src_y + y) % FB_H) * FB_W;
				src_off *= sizeof(uint16_t);

				/* Grab half word */
				half_word = gpu->vram[src_off] << 8;
				half_word |= gpu->vram[src_off + 1];

				/* Force masking bit (MSB) if required */
				if (gpu->stat.set_mask_bit)
					bitops_setw(&half_word, 15, 1, 1);

				/* Write bytes to frame buffer */
				gpu->vram[dest_off] = half_word >> 8;
				gpu->vram[dest_off + 1] = half_word;
			}
		}

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(A0h) - Copy Rectangle (CPU to VRAM) */
void cmd_copy_rect_cpu_to_vram(struct gpu *gpu)
{
	union cmd_copy_rect cmd;
	bool decoded;
	uint32_t data;
	uint32_t offset;
	uint16_t half_word;
	uint16_t x;
	uint16_t y;
	uint16_t w;
	uint16_t h;
	int i;

	/* Check if command is decoded */
	decoded = (gpu->fifo.cmd_half_word_count > 0);

	/* Retrieve command parameters if needed (3 arguments) */
	if (!decoded && !fifo_dequeue(&gpu->fifo, cmd.raw, 3))
		return;

	/* Intialize copy data and half word count */
	if (gpu->fifo.cmd_half_word_count == 0) {
		/* Get parameters from command, handling masking as such:
		Xpos = (Xpos AND 3FFh)           range 0..3FFh
		Ypos = (Ypos AND 1FFh)           range 0..1FFh
		Xsiz = ((Xsiz - 1) AND 3FFh) + 1 range 1..400h
		Ysiz = ((Ysiz - 1) AND 1FFh) + 1 range 1..200h */
		x = cmd.xy.x_coord & 0x3FF;
		y = cmd.xy.y_coord % 0x1FF;
		w = ((cmd.dimensions.width - 1) & 0x3FF) + 1;
		h = ((cmd.dimensions.height - 1) & 0x1FF) + 1;

		/* Save copy data parameters */
		gpu->copy_data.x = x;
		gpu->copy_data.y = y;
		gpu->copy_data.min_x = x;
		gpu->copy_data.min_y = y;
		gpu->copy_data.max_x = x + w;

		/* Save half word count based on pixel count */
		gpu->fifo.cmd_half_word_count = w * h;
	}

	/* Dequeue data */
	if (fifo_dequeue(&gpu->fifo, &data, 1)) {
		/* Copy two half words (data contains two pixels) */
		for (i = 0; i < 2; i++) {
			/* Compute destination offset in frame buffer */
			offset = gpu->copy_data.x % FB_W;
			offset += (gpu->copy_data.y % FB_H) * FB_W;
			offset *= sizeof(uint16_t);

			/* Get existing pixel data within frame buffer */
			half_word = gpu->vram[offset] << 8;
			half_word |= gpu->vram[offset + 1];

			/* The transfer is affected by mask setting */
			if (!gpu->stat.draw_pixels ||
				!bitops_getw(&half_word, 15, 1)) {
				/* Get appropriate half word */
				half_word = ((i % 2) == 0) ?
					data & 0xFFFF : data >> 16;

				/* Force masking bit (MSB) if required */
				if (gpu->stat.set_mask_bit)
					bitops_setw(&half_word, 15, 1, 1);

				/* Write bytes to frame buffer */
				gpu->vram[offset] = half_word >> 8;
				gpu->vram[offset + 1] = half_word;
			}

			/* Update destination coordinates (handling bounds) */
			if (++gpu->copy_data.x == gpu->copy_data.max_x) {
				gpu->copy_data.x = gpu->copy_data.min_x;
				gpu->copy_data.y++;
			}

			/* Decrement count and stop processing if needed */
			if (--gpu->fifo.cmd_half_word_count == 0) {
				gpu->fifo.cmd_in_progress = false;
				return;
			}
		}
	}
}

/* GP0(C0h) - Copy Rectangle (VRAM to CPU) */
void cmd_copy_rect_vram_to_cpu(struct gpu *gpu)
{
	union cmd_copy_rect cmd;
	struct read_buffer *read_buffer = &gpu->read_buffer;
	uint16_t x;
	uint16_t y;
	uint16_t w;
	uint16_t h;

	/* Retrieve command parameters (3 arguments) */
	if (!fifo_dequeue(&gpu->fifo, cmd.raw, 3))
		return;

	/* The send VRAM to CPU ready bit gets set after sending GP0(C0h) and
	its parameters, and stays set until all data words are received. */
	gpu->stat.ready_send_vram = 1;

	/* Save command for GPUREAD access */
	read_buffer->cmd = 0xC0;

	/* Get parameters from command, handling masking as such:
	Xpos = (Xpos AND 3FFh)           range 0..3FFh
	Ypos = (Ypos AND 1FFh)           range 0..1FFh
	Xsiz = ((Xsiz - 1) AND 3FFh) + 1 range 1..400h
	Ysiz = ((Ysiz - 1) AND 1FFh) + 1 range 1..200h */
	x = cmd.xy.x_coord & 0x3FF;
	y = cmd.xy.y_coord % 0x1FF;
	w = ((cmd.dimensions.width - 1) & 0x3FF) + 1;
	h = ((cmd.dimensions.height - 1) & 0x1FF) + 1;

	/* Save copy data parameters */
	read_buffer->copy_data.x = x;
	read_buffer->copy_data.y = y;
	read_buffer->copy_data.min_x = x;
	read_buffer->copy_data.min_y = y;
	read_buffer->copy_data.max_x = x + w;

	/* Save half word count based on pixel count */
	gpu->fifo.cmd_half_word_count = w * h;

	/* Flag command as complete */
	gpu->fifo.cmd_in_progress = false;
}

/* GP0(E1h) - Draw Mode setting (aka "Texpage") */
void cmd_draw_mode_setting(struct gpu *gpu, union cmd cmd)
{
	/* Save drawing mode parameters */
	gpu->stat.tex_page_x_base = cmd.draw_mode_setting.x_base;
	gpu->stat.tex_page_y_base = cmd.draw_mode_setting.y_base;
	gpu->stat.semi_transparency = cmd.draw_mode_setting.semi_transparency;
	gpu->stat.tex_page_colors = cmd.draw_mode_setting.colors;
	gpu->stat.dither = cmd.draw_mode_setting.dither;
	gpu->stat.drawing_allowed = cmd.draw_mode_setting.drawing_allowed;
	gpu->x_flip = cmd.draw_mode_setting.rect_x_flip;
	gpu->y_flip = cmd.draw_mode_setting.rect_y_flip;

	/* Allow texture disable flag change only if global flag is set */
	if (gpu->tex_disable)
		gpu->stat.tex_disable = cmd.draw_mode_setting.disable;
}

/* GP0(E2h) - Texture Window setting */
void cmd_tex_window_setting(struct gpu *gpu, union cmd cmd)
{
	/* Save texture window parameters */
	gpu->tex_window_mask_x = cmd.tex_window_setting.mask_x;
	gpu->tex_window_mask_y = cmd.tex_window_setting.mask_y;
	gpu->tex_window_offset_x = cmd.tex_window_setting.offset_x;
	gpu->tex_window_offset_y = cmd.tex_window_setting.offset_y;
}

/* GP0(E3h) - Set Drawing Area top left (X1,Y1) */
void cmd_set_drawing_area_tl(struct gpu *gpu, union cmd cmd)
{
	/* Update top-left X/Y drawing area coordinates */
	gpu->drawing_area_x1 = cmd.set_drawing_area.x_coord;
	gpu->drawing_area_y1 = cmd.set_drawing_area.y_coord;
}

/* GP0(E4h) - Set Drawing Area bottom right (X2,Y2) */
void cmd_set_drawing_area_br(struct gpu *gpu, union cmd cmd)
{
	/* Update bottom-right X/Y drawing area coordinates */
	gpu->drawing_area_x2 = cmd.set_drawing_area.x_coord;
	gpu->drawing_area_y2 = cmd.set_drawing_area.y_coord;
}

/* GP0(E5h) - Set Drawing Offset (X,Y) */
void cmd_set_drawing_offset(struct gpu *gpu, union cmd cmd)
{
	/* Update X/Y drawing offset */
	gpu->drawing_offset_x = (int16_t)cmd.set_drawing_offset.x_offset;
	gpu->drawing_offset_y = (int16_t)cmd.set_drawing_offset.y_offset;
}

/* GP0(E6h) - Mask Bit Setting */
void cmd_mask_bit_setting(struct gpu *gpu, union cmd cmd)
{
	/* Save mask bit parameters */
	gpu->stat.set_mask_bit = cmd.mask_bit_setting.set_while_drawing;
	gpu->stat.draw_pixels = cmd.mask_bit_setting.check_before_draw;
}

/* GP1(00h) - Reset GPU */
void cmd_reset_gpu(struct gpu *gpu)
{
	union cmd cmd;

	/* Reset status register */
	gpu->stat.raw = 0;
	gpu->stat.reserved = 1;
	gpu->stat.ready_recv_cmd = 1;
	gpu->stat.ready_recv_dma = 1;

	/* Resets the GPU to the following values:
	GP1(01h)      clear fifo
	GP1(02h)      ack irq (0)
	GP1(03h)      display off (1)
	GP1(04h)      dma off (0)
	GP1(05h)      display address (0)
	GP1(06h)      display x1, x2 (x1 = 200h, x2 = 200h + 256 * 10)
	GP1(07h)      display y1, y2 (y1 = 010h, y2 = 010h + 240)
	GP1(08h)      display mode 320x200 NTSC (0)
	GP0(E1h..E6h) rendering attributes (0) */

	/* Clear FIFO */
	cmd_reset_cmd_buffer(gpu);

	/* Acknowledge interrupt */
	cmd_ack_interrupt(gpu);

	/* Disable display */
	cmd.raw = 0;
	cmd.display_enable.display_on_off = 1;
	cmd_display_enable(gpu, cmd);

	/* Reset DMA direction (off) */
	cmd.raw = 0;
	cmd_dma_dir(gpu, cmd);

	/* Reset display address */
	cmd.raw = 0;
	cmd_start_of_display_area(gpu, cmd);

	/* Reset horizontal display range */
	cmd.raw = 0;
	cmd.horizontal_display_range.x1 = 0x200;
	cmd.horizontal_display_range.x2 = 0x200 + 256 * 10;
	cmd_horizontal_display_range(gpu, cmd);

	/* Reset vertical display range */
	cmd.raw = 0;
	cmd.vertical_display_range.y1 = 0x010;
	cmd.vertical_display_range.y2 = 0x010 + 240;
	cmd_vertical_display_range(gpu, cmd);

	/* Reset display mode */
	cmd.raw = 0;
	cmd_display_mode(gpu, cmd);

	/* Reset rendering attributes */
	cmd.raw = 0;
	cmd_draw_mode_setting(gpu, cmd);
	cmd_tex_window_setting(gpu, cmd);
	cmd_set_drawing_area_tl(gpu, cmd);
	cmd_set_drawing_area_br(gpu, cmd);
	cmd_set_drawing_offset(gpu, cmd);
	cmd_mask_bit_setting(gpu, cmd);
}

/* GP1(01h) - Reset Command Buffer */
void cmd_reset_cmd_buffer(struct gpu *gpu)
{
	/* Reset command buffer */
	gpu->fifo.pos = 0;
	gpu->fifo.num = 0;
	gpu->fifo.cmd_in_progress = false;
}

/* GP1(02h) - Acknowledge GPU Interrupt (IRQ1) */
void cmd_ack_interrupt(struct gpu *gpu)
{
	/* Acknowledge IRQ flag in status register */
	gpu->stat.irq = 0;
}

/* GP1(03h) - Display Enable */
void cmd_display_enable(struct gpu *gpu, union cmd cmd)
{
	/* Enable/disable display */
	gpu->stat.display_disable = cmd.display_enable.display_on_off;
}

/* GP1(04h) - DMA Direction / Data Request */
void cmd_dma_dir(struct gpu *gpu, union cmd cmd)
{
	/* Save DMA direction */
	gpu->stat.dma_dir = cmd.dma_dir.dir;

	/* Update status register (DMA / Data Request meaning depends on
	GP1(04h) DMA Direction:
		When GP1(04h) = 0 ---> Always zero (0)
		When GP1(04h) = 1 ---> FIFO State  (0 = Full, 1 = Not Full)
		When GP1(04h) = 2 ---> Same as GPUSTAT.28
		When GP1(04h) = 3 ---> Same as GPUSTAT.27 */
	switch (gpu->stat.dma_dir) {
	case DMA_DIR_OFF:
		gpu->stat.dma_data_req = 0;
		break;
	case DMA_DIR_FIFO_STATUS:
		gpu->stat.dma_data_req = !fifo_full(&gpu->fifo);
		break;
	case DMA_DIR_CPU_TO_GP0:
		gpu->stat.dma_data_req = gpu->stat.ready_recv_dma;
		break;
	case DMA_DIR_GPUREAD_TO_CPU:
		gpu->stat.dma_data_req = gpu->stat.ready_send_vram;
		break;
	}
}

/* GP1(05h) - Start of Display area (in VRAM) */
void cmd_start_of_display_area(struct gpu *gpu, union cmd cmd)
{
	/* Save display area origin */
	gpu->display_area_src_x = cmd.start_of_display_area.x;
	gpu->display_area_src_y = cmd.start_of_display_area.y;
}

/* GP1(06h) - Horizontal Display range (on Screen) */
void cmd_horizontal_display_range(struct gpu *gpu, union cmd cmd)
{
	/* Save horizontal display range */
	gpu->display_area_dest_x1 = cmd.horizontal_display_range.x1;
	gpu->display_area_dest_x2 = cmd.horizontal_display_range.x2;
}

/* GP1(07h) - Vertical Display range (on Screen) */
void cmd_vertical_display_range(struct gpu *gpu, union cmd cmd)
{
	/* Save horizontal display range */
	gpu->display_area_dest_y1 = cmd.vertical_display_range.y1;
	gpu->display_area_dest_y2 = cmd.vertical_display_range.y2;
}

/* GP1(08h) - Display mode */
void cmd_display_mode(struct gpu *gpu, union cmd cmd)
{
	int prev_w;
	int prev_h;
	int w;
	int h;

	/* Get current screen size */
	video_get_size(&prev_w, &prev_h);

	/* Save parameters */
	gpu->stat.horizontal_res_1 = cmd.display_mode.horizontal_res_1;
	gpu->stat.vertical_res = cmd.display_mode.vertical_res;
	gpu->stat.video_mode = cmd.display_mode.video_mode;
	gpu->stat.color_depth = cmd.display_mode.color_depth;
	gpu->stat.vertical_interlace = cmd.display_mode.vertical_interlace;
	gpu->stat.horizontal_res_2 = cmd.display_mode.horizontal_res_2;
	gpu->stat.reverse = cmd.display_mode.reverse;

	/* Set horizontal resolution based on the following parameters:
	Horizontal Resolution 1 (0 = 256, 1 = 320, 2 = 512, 3 = 640)
	Horizontal Resolution 2 (0 = 256/320/512/640, 1 = 368) */
	if (gpu->stat.horizontal_res_2 == 0)
		switch (gpu->stat.horizontal_res_1) {
		case 0:
			w = 256;
			break;
		case 1:
			w = 320;
			break;
		case 2:
			w = 512;
			break;
		case 3:
			w = 640;
			break;
		}
	else
		w = 368;

	/* Set vertical resolution based on the following parameter:
	Vertical Resolution (0 = 240, 1 = 480, when Vertical Interlace = 1)
	Vertical Interlace (0 = Off, 1 = On) */
	if (gpu->stat.vertical_interlace == 1)
		switch (gpu->stat.vertical_res) {
		case 0:
			h = 240;
			break;
		case 1:
			h = 480;
			break;
		}
	else
		h = 240;

	/* Update screen resolution if requested */
	if ((w != prev_w) || (h != prev_h))
		video_set_size(w, h);
}

/* GP1(09h) - Texture Disable */
void cmd_texture_disable(struct gpu *gpu, union cmd cmd)
{
	/* Save texture disable flag */
	gpu->tex_disable = cmd.texture_disable.disable;
}

/* GP1(10h) - Get GPU Info
   GP1(11h..1Fh) - Mirrors of GP1(10h), Get GPU Info */
void cmd_get_gpu_info(struct gpu *gpu, union cmd cmd)
{
	union cmd result;

	/* Save command for GPUREAD access */
	gpu->read_buffer.cmd = 0x10;

	/* Fill GPUREAD buffer based on requested information */
	result.raw = 0;
	switch (cmd.get_gpu_info.info) {
	case 0x02:
		/* Read texture window setting (GP0(E2h)) */
		result.tex_window_setting.mask_x = gpu->tex_window_mask_x;
		result.tex_window_setting.mask_y = gpu->tex_window_mask_y;
		result.tex_window_setting.offset_x = gpu->tex_window_offset_x;
		result.tex_window_setting.offset_y = gpu->tex_window_offset_y;
		gpu->read_buffer.data = result.raw;
		break;
	case 0x03:
		/* Read draw area top left (GP0(E3h)) */
		result.set_drawing_area.x_coord = gpu->drawing_area_x1;
		result.set_drawing_area.y_coord = gpu->drawing_area_y1;
		gpu->read_buffer.data = result.raw;
		break;
	case 0x04:
		/* Read draw area bottom right (GP0(E4h)) */
		result.set_drawing_area.x_coord = gpu->drawing_area_x2;
		result.set_drawing_area.y_coord = gpu->drawing_area_y2;
		gpu->read_buffer.data = result.raw;
		break;
	case 0x05:
		/* Read draw offset (GP0(E5h)) */
		result.set_drawing_offset.x_offset = gpu->drawing_offset_x;
		result.set_drawing_offset.y_offset = gpu->drawing_offset_y;
		gpu->read_buffer.data = result.raw;
		break;
	case 0x07:
		/* Read GPU type */
		gpu->read_buffer.data = GPU_TYPE;
		break;
	case 0x08:
		/* Unknown (returns 00000000h) */
		gpu->read_buffer.data = 0;
		break;
	case 0x00:
	case 0x01:
	case 0x06:
	case 0x09:
	case 0x0A:
	case 0x0B:
	case 0x0C:
	case 0x0D:
	case 0x0E:
	case 0x0F:
	default:
		/* Returns nothing (old value in GPUREAD remains unchanged) */
		break;
	}
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
	struct read_buffer *read_buffer = &gpu->read_buffer;
	struct copy_data *copy_data = &read_buffer->copy_data;
	uint32_t data;
	uint32_t offset;
	uint16_t half_word;
	int i;

	/* Handle read */
	switch (address) {
	case GPUREAD:
		switch (read_buffer->cmd) {
		case 0xC0:
			/* Handle GP0(C0h) VRAM to CPU copy */
			data = 0;
			for (i = 0; i < 2; i++) {
				/* Compute destination offset in frame buffer */
				offset = copy_data->x % FB_W;
				offset += (copy_data->y % FB_H) * FB_W;
				offset *= sizeof(uint16_t);

				/* Get half word from frame buffer */
				half_word = gpu->vram[offset] << 8;
				half_word |= gpu->vram[offset + 1];

				/* Set appropriate data half word */
				bitops_setl(&data, i * 16, 16, half_word);

				/* Update X coordinate */
				copy_data->x++;

				/* Decrement count */
				if (gpu->fifo.cmd_half_word_count > 0)
					gpu->fifo.cmd_half_word_count--;

				/* Handle completion */
				if (gpu->fifo.cmd_half_word_count == 0) {
					/* Reset saved command */
					read_buffer->cmd = 0;

					/* Skip bound check */
					continue;
				}

				/* Update coordinates (handling bounds) */
				if (copy_data->x == copy_data->max_x) {
					copy_data->x = copy_data->min_x;
					copy_data->y++;
				}
			}
			break;
		default:
			/* Get data from read buffer */
			data = read_buffer->data;
			break;
		}
		break;
	case GPUSTAT:
	default:
		/* Get status register */
		data = gpu->stat.raw;
		break;
	}

	/* Return filled data */
	return data;
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
	case 0x01:
		cmd_clear_cache(gpu);
		break;
	case 0x02:
		cmd_fill_rectangle(gpu);
		break;
	case 0x20:
	case 0x21:
		cmd_monochrome_3p_poly(gpu, true);
		break;
	case 0x22:
	case 0x23:
		cmd_monochrome_3p_poly(gpu, false);
		break;
	case 0x24:
		cmd_textured_3p_poly(gpu, true, false);
		break;
	case 0x25:
		cmd_textured_3p_poly(gpu, true, true);
		break;
	case 0x26:
		cmd_textured_3p_poly(gpu, false, false);
		break;
	case 0x27:
		cmd_textured_3p_poly(gpu, false, true);
		break;
	case 0x28:
	case 0x29:
		cmd_monochrome_4p_poly(gpu, true);
		break;
	case 0x2A:
	case 0x2B:
		cmd_monochrome_4p_poly(gpu, false);
		break;
	case 0x2C:
		cmd_textured_4p_poly(gpu, true, false);
		break;
	case 0x2D:
		cmd_textured_4p_poly(gpu, true, true);
		break;
	case 0x2E:
		cmd_textured_4p_poly(gpu, false, false);
		break;
	case 0x2F:
		cmd_textured_4p_poly(gpu, false, true);
		break;
	case 0x30:
	case 0x31:
		cmd_shaded_3p_poly(gpu, true);
		break;
	case 0x32:
	case 0x33:
		cmd_shaded_3p_poly(gpu, false);
		break;
	case 0x34:
	case 0x35:
		cmd_shaded_textured_3p_poly(gpu, true);
		break;
	case 0x36:
	case 0x37:
		cmd_shaded_textured_3p_poly(gpu, false);
		break;
	case 0x38:
	case 0x39:
		cmd_shaded_4p_poly(gpu, true);
		break;
	case 0x3A:
	case 0x3B:
		cmd_shaded_4p_poly(gpu, false);
		break;
	case 0x3C:
	case 0x3D:
		cmd_shaded_textured_4p_poly(gpu, true);
		break;
	case 0x3E:
	case 0x3F:
		cmd_shaded_textured_4p_poly(gpu, false);
		break;
	case 0x40:
	case 0x41:
	case 0x44:
	case 0x45:
		cmd_monochrome_line(gpu, true);
		break;
	case 0x42:
	case 0x43:
	case 0x46:
	case 0x47:
		cmd_monochrome_line(gpu, false);
		break;
	case 0x48:
	case 0x49:
	case 0x4C:
	case 0x4D:
		cmd_monochrome_poly_line(gpu, true);
		break;
	case 0x4A:
	case 0x4B:
	case 0x4E:
	case 0x4F:
		cmd_monochrome_poly_line(gpu, false);
		break;
	case 0x50:
	case 0x51:
	case 0x54:
	case 0x55:
		cmd_shaded_line(gpu, true);
		break;
	case 0x52:
	case 0x53:
	case 0x56:
	case 0x57:
		cmd_shaded_line(gpu, false);
		break;
	case 0x58:
	case 0x59:
	case 0x5C:
	case 0x5D:
		cmd_shaded_poly_line(gpu, true);
		break;
	case 0x5A:
	case 0x5B:
	case 0x5E:
	case 0x5F:
		cmd_shaded_poly_line(gpu, false);
		break;
	case 0x60:
	case 0x61:
		cmd_monochrome_rect_var(gpu, true);
		break;
	case 0x62:
	case 0x63:
		cmd_monochrome_rect_var(gpu, false);
		break;
	case 0x64:
		cmd_tex_rect_var(gpu, true, false);
		break;
	case 0x65:
		cmd_tex_rect_var(gpu, true, true);
		break;
	case 0x66:
		cmd_tex_rect_var(gpu, false, false);
		break;
	case 0x67:
		cmd_tex_rect_var(gpu, false, true);
		break;
	case 0x68:
	case 0x69:
		cmd_monochrome_rect_1x1(gpu, true);
		break;
	case 0x6A:
	case 0x6B:
		cmd_monochrome_rect_1x1(gpu, false);
		break;
	case 0x6C:
		cmd_tex_rect_1x1(gpu, true, false);
		break;
	case 0x6D:
		cmd_tex_rect_1x1(gpu, true, true);
		break;
	case 0x6E:
		cmd_tex_rect_1x1(gpu, false, false);
		break;
	case 0x6F:
		cmd_tex_rect_1x1(gpu, false, true);
		break;
	case 0x70:
	case 0x71:
		cmd_monochrome_rect_8x8(gpu, true);
		break;
	case 0x72:
	case 0x73:
		cmd_monochrome_rect_8x8(gpu, false);
		break;
	case 0x74:
		cmd_tex_rect_8x8(gpu, true, false);
		break;
	case 0x75:
		cmd_tex_rect_8x8(gpu, true, true);
		break;
	case 0x76:
		cmd_tex_rect_8x8(gpu, false, false);
		break;
	case 0x77:
		cmd_tex_rect_8x8(gpu, false, true);
		break;
	case 0x78:
	case 0x79:
		cmd_monochrome_rect_16x16(gpu, true);
		break;
	case 0x7A:
	case 0x7B:
		cmd_monochrome_rect_16x16(gpu, false);
		break;
	case 0x7C:
		cmd_tex_rect_16x16(gpu, true, false);
		break;
	case 0x7D:
		cmd_tex_rect_16x16(gpu, true, true);
		break;
	case 0x7E:
		cmd_tex_rect_16x16(gpu, false, false);
		break;
	case 0x7F:
		cmd_tex_rect_16x16(gpu, false, true);
		break;
	case 0x80:
	case 0x81:
	case 0x82:
	case 0x83:
	case 0x84:
	case 0x85:
	case 0x86:
	case 0x87:
	case 0x88:
	case 0x89:
	case 0x8A:
	case 0x8B:
	case 0x8C:
	case 0x8D:
	case 0x8E:
	case 0x8F:
	case 0x90:
	case 0x91:
	case 0x92:
	case 0x93:
	case 0x94:
	case 0x95:
	case 0x96:
	case 0x97:
	case 0x98:
	case 0x99:
	case 0x9A:
	case 0x9B:
	case 0x9C:
	case 0x9D:
	case 0x9E:
	case 0x9F:
		cmd_copy_rect_vram_to_vram(gpu);
		break;
	case 0xA0:
	case 0xA1:
	case 0xA2:
	case 0xA3:
	case 0xA4:
	case 0xA5:
	case 0xA6:
	case 0xA7:
	case 0xA8:
	case 0xA9:
	case 0xAA:
	case 0xAB:
	case 0xAC:
	case 0xAD:
	case 0xAE:
	case 0xAF:
	case 0xB0:
	case 0xB1:
	case 0xB2:
	case 0xB3:
	case 0xB4:
	case 0xB5:
	case 0xB6:
	case 0xB7:
	case 0xB8:
	case 0xB9:
	case 0xBA:
	case 0xBB:
	case 0xBC:
	case 0xBD:
	case 0xBE:
	case 0xBF:
		cmd_copy_rect_cpu_to_vram(gpu);
		break;
	case 0xC0:
	case 0xC1:
	case 0xC2:
	case 0xC3:
	case 0xC4:
	case 0xC5:
	case 0xC6:
	case 0xC7:
	case 0xC8:
	case 0xC9:
	case 0xCA:
	case 0xCB:
	case 0xCC:
	case 0xCD:
	case 0xCE:
	case 0xCF:
	case 0xD0:
	case 0xD1:
	case 0xD2:
	case 0xD3:
	case 0xD4:
	case 0xD5:
	case 0xD6:
	case 0xD7:
	case 0xD8:
	case 0xD9:
	case 0xDA:
	case 0xDB:
	case 0xDC:
	case 0xDD:
	case 0xDE:
	case 0xDF:
		cmd_copy_rect_vram_to_cpu(gpu);
		break;
	default:
		LOG_W("Unhandled GP0 opcode (%02x)!\n", fifo->cmd_opcode);
		break;
	}
}

void gpu_gp0_cmd(struct gpu *gpu, union cmd cmd)
{
	/* Handle immediate commands and leave if no command is in progress */
	if (!gpu->fifo.cmd_in_progress)
		switch (cmd.opcode) {
		case 0x00:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C:
		case 0x0D:
		case 0x0E:
		case 0x0F:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1A:
		case 0x1B:
		case 0x1C:
		case 0x1D:
		case 0x1E:
		case 0xE0:
		case 0xE7:
		case 0xE8:
		case 0xE9:
		case 0xEA:
		case 0xEB:
		case 0xEC:
		case 0xED:
		case 0xEE:
		case 0xEF:
			cmd_nop();
			return;
		case 0xE1:
			cmd_draw_mode_setting(gpu, cmd);
			return;
		case 0xE2:
			cmd_tex_window_setting(gpu, cmd);
			return;
		case 0xE3:
			cmd_set_drawing_area_tl(gpu, cmd);
			return;
		case 0xE4:
			cmd_set_drawing_area_br(gpu, cmd);
			return;
		case 0xE5:
			cmd_set_drawing_offset(gpu, cmd);
			return;
		case 0xE6:
			cmd_mask_bit_setting(gpu, cmd);
			return;
		default:
			break;
		}

	/* Add command/data to FIFO */
	fifo_enqueue(&gpu->fifo, cmd.raw);

	/* Process FIFO commands */
	gpu_process_fifo(gpu);

	/* Update DMA / Data Request status bit if needed */
	if (gpu->stat.dma_dir == DMA_DIR_FIFO_STATUS)
		gpu->stat.dma_data_req = !fifo_full(&gpu->fifo);
}

void gpu_gp1_cmd(struct gpu *gpu, union cmd cmd)
{
	/* Execute command */
	switch (cmd.opcode) {
	case 0x00:
		cmd_reset_gpu(gpu);
		break;
	case 0x01:
		cmd_reset_cmd_buffer(gpu);
		break;
	case 0x02:
		cmd_ack_interrupt(gpu);
		break;
	case 0x03:
		cmd_display_enable(gpu, cmd);
		break;
	case 0x04:
		cmd_dma_dir(gpu, cmd);
		break;
	case 0x05:
		cmd_start_of_display_area(gpu, cmd);
		break;
	case 0x06:
		cmd_horizontal_display_range(gpu, cmd);
		break;
	case 0x07:
		cmd_vertical_display_range(gpu, cmd);
		break;
	case 0x08:
		cmd_display_mode(gpu, cmd);
		break;
	case 0x09:
		cmd_texture_disable(gpu, cmd);
		break;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
	case 0x18:
	case 0x19:
	case 0x1A:
	case 0x1B:
	case 0x1C:
	case 0x1D:
	case 0x1E:
	case 0x1F:
		cmd_get_gpu_info(gpu, cmd);
		break;
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

