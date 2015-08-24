#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <caca.h>
#include <log.h>
#include <util.h>
#include <video.h>

#define CHAR_WIDTH	8
#define CHAR_HEIGHT	16

#define BPP 32
#define R_MASK	0x00FF0000
#define R_SHIFT	16
#define G_MASK	0x0000FF00
#define G_SHIFT	8
#define B_MASK	0x000000FF
#define B_SHIFT	0
#define A_MASK	0x00000000

struct caca_data {
	caca_display_t *dp;
	int width;
	int height;
	uint32_t *pixels;
	caca_dither_t *dither;
};

static window_t *caca_init(struct video_frontend *fe, struct video_specs *vs);
static void caca_update(struct video_frontend *fe);
static struct color caca_get_p(struct video_frontend *fe, int x, int y);
static void caca_set_p(struct video_frontend *fe, int x, int y, struct color c);
static void caca_deinit(struct video_frontend *fe);

window_t *caca_init(struct video_frontend *fe, struct video_specs *vs)
{
	caca_canvas_t *cv;
	caca_display_t *dp;
	struct caca_data *data;
	int pitch;
	int w = vs->width;
	int h = vs->height;
	int s = vs->scale;

	/* Create canvas and display */
	cv = caca_create_canvas(w * s / CHAR_WIDTH, h * s / CHAR_HEIGHT);
	dp = caca_create_display(cv);
	if (!dp) {
		LOG_E("Could not create caca display!\n");
		return NULL;
	}

	/* Set window title */
	caca_set_display_title(dp, "emux");
	caca_refresh_display(dp);

	/* Create private data */
	data = calloc(1, sizeof(struct caca_data));
	data->dp = dp;
	data->width = w;
	data->height = h;
	fe->priv_data = data;

	/* Initialize pixels */
	data->pixels = calloc(w * h, sizeof(uint32_t));

	/* Initialize dither */
	pitch = (BPP / 8) * w;
	data->dither = caca_create_dither(BPP, w, h, pitch, R_MASK, G_MASK,
		B_MASK, A_MASK);

	return dp;
}

void caca_update(struct video_frontend *fe)
{
	struct caca_data *data = fe->priv_data;
	caca_display_t *dp = data->dp;
	caca_canvas_t *cv = caca_get_canvas(dp);

	/* Dither pixels and fill canvas */
	caca_dither_bitmap(cv, 0, 0, caca_get_canvas_width(cv),
		caca_get_canvas_height(cv), data->dither, data->pixels);

	caca_refresh_display(dp);
}

struct color caca_get_p(struct video_frontend *fe, int x, int y)
{
	struct caca_data *data = fe->priv_data;
	uint32_t pixel = data->pixels[x + y * data->width];
	struct color c;
	c.r = (pixel & R_MASK) >> R_SHIFT;
	c.g = (pixel & G_MASK) >> G_SHIFT;
	c.b = (pixel & B_MASK) >> B_SHIFT;
	return c;
}

void caca_set_p(struct video_frontend *fe, int x, int y, struct color c)
{
	struct caca_data *data = fe->priv_data;
	uint32_t pixel = 0;
	pixel |= c.r << R_SHIFT;
	pixel |= c.g << G_SHIFT;
	pixel |= c.b << B_SHIFT;
	data->pixels[x + y * data->width] = pixel;
}

void caca_deinit(struct video_frontend *fe)
{
	struct caca_data *data = fe->priv_data;
	caca_display_t *dp = data->dp;

	caca_free_canvas(caca_get_canvas(dp));
	caca_free_display(dp);
	free(data->pixels);
	free(data);
}

VIDEO_START(caca)
	.input = "caca",
	.init = caca_init,
	.update = caca_update,
	.get_p = caca_get_p,
	.set_p = caca_set_p,
	.deinit = caca_deinit
VIDEO_END

