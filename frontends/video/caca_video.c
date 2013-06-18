#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <caca.h>
#include <util.h>
#include <video.h>

#define BPP 32
#define R_MASK	0x00FF0000
#define R_SHIFT	16
#define G_MASK	0x0000FF00
#define G_SHIFT	8
#define B_MASK	0x000000FF
#define B_SHIFT	0
#define A_MASK	0x00000000

struct caca_surface {
	int width;
	int height;
	uint32_t *pixels;
	caca_dither_t *dither;
};

static video_window_t *caca_init(int width, int height, int scale);
static void caca_update();
static uint32_t caca_map_rgb(uint8_t r, uint8_t g, uint8_t b);
static uint32_t caca_get_pixel(int x, int y);
static void caca_set_pixel(int x, int y, uint32_t pixel);
static void caca_deinit();

static caca_display_t *dp;
static struct caca_surface *surface;

video_window_t *caca_init(int width, int height, int scale)
{
	caca_canvas_t *cv;
	int pitch;

	/* Create canvas and display */
	cv = caca_create_canvas(width * scale, height * scale);
	dp = caca_create_display(cv);
	if (!dp) {
		fprintf(stderr, "Can't create caca display!\n");
		return NULL;
	}

	/* Set window title */
	caca_set_display_title(dp, "emux");
	caca_refresh_display(dp);

	/* Create surface and assign dimensions */
	surface = malloc(sizeof(struct caca_surface));
	surface->width = width;
	surface->height = height;

	/* Initialize pixels */
	surface->pixels = malloc(width * height * sizeof(uint32_t));
	memset(surface->pixels, 0, width * height * sizeof(uint32_t));

	/* Initialize dither */
	pitch = (BPP / 8) * width;
	surface->dither = caca_create_dither(BPP, width, height, pitch, R_MASK,
		G_MASK, B_MASK, A_MASK);

	return dp;
}

void caca_update()
{
	caca_canvas_t *cv = caca_get_canvas(dp);

	/* Dither pixels and fill canvas */
	caca_dither_bitmap(cv, 0, 0, caca_get_canvas_width(cv),
		caca_get_canvas_height(cv), surface->dither, surface->pixels);

	caca_refresh_display(dp);
}

uint32_t caca_map_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	return (r << R_SHIFT) | (g << G_SHIFT) | (b << B_SHIFT);
}

uint32_t caca_get_pixel(int x, int y)
{
	return surface->pixels[x + y * surface->width];
}

void caca_set_pixel(int x, int y, uint32_t pixel)
{
	surface->pixels[x + y * surface->width] = pixel;
}

void caca_deinit()
{
	caca_free_canvas(caca_get_canvas(dp));
	caca_free_display(dp);
}

VIDEO_START(caca)
	.init = caca_init,
	.update = caca_update,
	.map_rgb = caca_map_rgb,
	.get_pixel = caca_get_pixel,
	.set_pixel = caca_set_pixel,
	.deinit = caca_deinit
VIDEO_END

