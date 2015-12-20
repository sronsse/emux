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

static struct mops gpu_mops = {
	.readl = (readl_t)gpu_readl,
	.writel = (writel_t)gpu_writel
};

static struct dma_ops gpu_dma_ops = {
	.readl = (dma_readl_t)gpu_dma_readl,
	.writel = (dma_writel_t)gpu_dma_writel
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

