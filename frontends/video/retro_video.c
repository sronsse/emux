#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libretro.h>
#include <log.h>
#include <util.h>
#include <video.h>

#define BPP 32
#define R_MASK	0x00FF0000
#define R_SHIFT	16
#define G_MASK	0x0000FF00
#define G_SHIFT	8
#define B_MASK	0x000000FF
#define B_SHIFT	0

struct retro_data {
	uint32_t *pixels;
	int width;
	int height;
	double fps;
	retro_video_refresh_t video_cb;
	bool video_updated;
};

void retro_video_fill_timing(struct retro_system_timing *timing);
void retro_video_fill_geometry(struct retro_game_geometry *geometry);
bool retro_video_updated();

static window_t *ret_init(struct video_frontend *fe, struct video_specs *vs);
static void ret_update(struct video_frontend *fe);
static struct color ret_get_p(struct video_frontend *fe, int x, int y);
static void ret_set_p(struct video_frontend *fe, int x, int y, struct color c);
static void ret_deinit(struct video_frontend *fe);

extern retro_environment_t retro_environment_cb;
static struct retro_data retro_data;

void retro_set_video_refresh(retro_video_refresh_t cb)
{
	/* Save video callback */
	retro_data.video_cb = cb;
}

void retro_video_fill_timing(struct retro_system_timing *timing)
{
	/* Fill FPS */
	timing->fps = retro_data.fps;
}

void retro_video_fill_geometry(struct retro_game_geometry *geometry)
{
	/* Fill geometry */
	geometry->base_width = retro_data.width;
	geometry->base_height = retro_data.height;
	geometry->max_width = retro_data.width;
	geometry->max_height = retro_data.height;
}

bool retro_video_updated()
{
	bool ret;

	/* Get current state */
	ret = retro_data.video_updated;

	/* Reset state if needed */
	if (retro_data.video_updated)
		retro_data.video_updated = 0;

	/* Return old state */
	return ret;
}

window_t *ret_init(struct video_frontend *UNUSED(fe), struct video_specs *vs)
{
	/* Set pixel format */
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
	if (!retro_environment_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
		LOG_E("Could not set pixel format!\n");
		return NULL;
	}

	/* Initialize pixels */
	retro_data.pixels = malloc(vs->width * vs->height * sizeof(uint32_t));
	memset(retro_data.pixels, 0, vs->width * vs->height * sizeof(uint32_t));

	/* Save dimensions and FPS */
	retro_data.width = vs->width;
	retro_data.height = vs->height;
	retro_data.fps = vs->fps;

	/* Return success (no window is returned) */
	return (window_t *)1;
}

void ret_update(struct video_frontend *UNUSED(fe))
{
	int pitch;

	/* Refresh screen */
	pitch = retro_data.width * (BPP / 8);
	retro_data.video_cb(retro_data.pixels,
		retro_data.width,
		retro_data.height,
		pitch);

	/* Flag that video has been updated */
	retro_data.video_updated = true;
}

struct color ret_get_p(struct video_frontend *UNUSED(fe), int x, int y)
{
	uint32_t pixel = retro_data.pixels[x + y * retro_data.width];
	struct color c;
	c.r = (pixel & R_MASK) >> R_SHIFT;
	c.g = (pixel & G_MASK) >> G_SHIFT;
	c.b = (pixel & B_MASK) >> B_SHIFT;
	return c;
}

void ret_set_p(struct video_frontend *UNUSED(fe), int x, int y, struct color c)
{
	uint32_t pixel = 0;
	pixel |= c.r << R_SHIFT;
	pixel |= c.g << G_SHIFT;
	pixel |= c.b << B_SHIFT;
	retro_data.pixels[x + y * retro_data.width] = pixel;
}

void ret_deinit(struct video_frontend *UNUSED(fe))
{
	free(retro_data.pixels);
}

VIDEO_START(retro)
	.input = "retro",
	.init = ret_init,
	.update = ret_update,
	.get_p = ret_get_p,
	.set_p = ret_set_p,
	.deinit = ret_deinit
VIDEO_END

