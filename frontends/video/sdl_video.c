#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <log.h>
#include <util.h>
#include <video.h>

struct sdl_data {
	SDL_Surface *screen;
	int scale;
};

static window_t *sdl_init(struct video_frontend *fe, struct video_specs *vs);
static void sdl_update(struct video_frontend *fe);
static void sdl_lock(struct video_frontend *fe);
static void sdl_unlock(struct video_frontend *fe);
static struct color sdl_get_p(struct video_frontend *fe, int x, int y);
static void sdl_set_p(struct video_frontend *fe, int x, int y, struct color c);
static void sdl_deinit(struct video_frontend *fe);

window_t *sdl_init(struct video_frontend *fe, struct video_specs *vs)
{
	struct sdl_data *data;
	SDL_Surface *screen;
	Uint32 flags = SDL_SWSURFACE;
	int w = vs->width * vs->scale;
	int h = vs->height * vs->scale;

	/* Initialize video sub-system */
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
		LOG_E("Error initializing SDL video: %s\n", SDL_GetError());
		return NULL;
	}

	/* Set window position and title */
	SDL_putenv("SDL_VIDEO_CENTERED=center");
	SDL_WM_SetCaption("emux", NULL);

	/* Create main video surface */
	screen = SDL_SetVideoMode(w, h, 0, flags);
	if (!screen) {
		LOG_E("Error creating video surface: %s\n", SDL_GetError());
		SDL_VideoQuit();
		return NULL;
	}

	/* Create and fill private data */
	data = malloc(sizeof(struct sdl_data));
	data->screen = screen;
	data->scale = vs->scale;
	fe->priv_data = data;

	return screen;
}

void sdl_update(struct video_frontend *fe)
{
	struct sdl_data *data = fe->priv_data;
	SDL_Flip(data->screen);
}

void sdl_lock(struct video_frontend *fe)
{
	struct sdl_data *data = fe->priv_data;
	SDL_Surface *screen = data->screen;
	if (SDL_MUSTLOCK(screen) && (SDL_LockSurface(screen) < 0))
		LOG_W("Couldn't lock surface: %s\n", SDL_GetError());
}

void sdl_unlock(struct video_frontend *fe)
{
	struct sdl_data *data = fe->priv_data;
	SDL_Surface *screen = data->screen;
	if (SDL_MUSTLOCK(screen))
		SDL_UnlockSurface(screen);
}

struct color sdl_get_p(struct video_frontend *fe, int x, int y)
{
	struct sdl_data *data = fe->priv_data;
	SDL_Surface *screen = data->screen;
	int bpp = screen->format->BytesPerPixel;
	uint8_t *p;
	uint32_t pixel;
	struct color color;

	/* Apply scaling factor to coordinates and get pixel pointer */
	x *= data->scale;
	y *= data->scale;
	p = (uint8_t *)screen->pixels + y * screen->pitch + x * bpp;

	/* Read pixel */
	switch (bpp) {
	case 1:
		pixel = *p;
		break;
	case 2:
		pixel = *(uint16_t *)p;
		break;
	case 3:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
			pixel = (p[0] << 16) | (p[1] << 8) | p[2];
		else
			pixel = p[0] | (p[1] << 8) | (p[2] << 16);
		break;
	case 4:
		pixel = *(uint32_t *)p;
		break;
	default:
		pixel = 0;
		break;
	}

	/* Get RGB components */
	SDL_GetRGB(pixel, screen->format, &color.r, &color.g, &color.b);
	return color;
}

void sdl_set_p(struct video_frontend *fe, int x, int y, struct color c)
{
	struct sdl_data *data = fe->priv_data;
	SDL_Surface *screen = data->screen;
	uint32_t pixel;
	int bpp = screen->format->BytesPerPixel;
	uint8_t *p1;
	uint8_t *p2;
	int i;
	int j;

	/* Map color */
	pixel = SDL_MapRGB(screen->format, c.r, c.g, c.b);

	/* Apply scaling factor to coordinates */
	x *= data->scale;
	y *= data->scale;

	/* Set pixel contents */
	p1 = (uint8_t *)screen->pixels + y * screen->pitch + x * bpp;
	switch (bpp) {
	case 1:
		*p1 = pixel;
		break;
	case 2:
		*(uint16_t *)p1 = pixel;
		break;
	case 3:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			p1[0] = (pixel >> 16) & 0xFF;
			p1[1] = (pixel >> 8) & 0xFF;
			p1[2] = pixel & 0xFF;
		} else {
			p1[0] = pixel & 0xFF;
			p1[1] = (pixel >> 8) & 0xFF;
			p1[2] = (pixel >> 16) & 0xFF;
		}
		break;
	case 4:
		*(uint32_t *)p1 = pixel;
		break;
	default:
		break;
	}

	/* Write remaining square of pixels depending on scaling factor */
	for (i = x; i < x + data->scale; i++)
		for (j = y; j < y + data->scale; j++) {
			/* Skip source pixel */
			if ((i == x) && (j == y))
				continue;

			/* Compute pixel pointer */
			p2 = (uint8_t *)screen->pixels + j * screen->pitch +
				i * bpp;

			/* Copy previous pixel contents */
			memcpy(p2, p1, bpp);
		}
}

void sdl_deinit(struct video_frontend *fe)
{
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	free(fe->priv_data);
}

VIDEO_START(sdl)
	.input = "sdl",
	.init = sdl_init,
	.update = sdl_update,
	.lock = sdl_lock,
	.unlock = sdl_unlock,
	.get_p = sdl_get_p,
	.set_p = sdl_set_p,
	.deinit = sdl_deinit
VIDEO_END

