#include <stdio.h>
#include <stdbool.h>
#include <SDL.h>
#include <log.h>
#include <video.h>

static bool sdl_init(int width, int height, int scale);
static video_window_t *sdl_get_window();
static void sdl_update();
static void sdl_lock();
static void sdl_unlock();
static struct color sdl_get_pixel(int x, int y);
static void sdl_set_pixel(int x, int y, struct color color);
static void sdl_deinit();

static SDL_Surface *screen;
static int scale_factor;

bool sdl_init(int width, int height, int scale)
{
	Uint32 flags = SDL_SWSURFACE;

	/* Initialize video sub-system */
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
		LOG_E("Error initializing SDL video: %s\n", SDL_GetError());
		return false;
	}

	/* Set window position and title */
	SDL_putenv("SDL_VIDEO_CENTERED=center");
	SDL_WM_SetCaption("emux", NULL);

	/* Create main video surface */
	screen = SDL_SetVideoMode(width * scale, height * scale, 0, flags);
	if (!screen) {
		LOG_E("Error creating video surface: %s\n", SDL_GetError());
		SDL_VideoQuit();
		return false;
	}

	/* Save scaling factor */
	scale_factor = scale;

	return true;
}

video_window_t *sdl_get_window()
{
	return screen;
}

void sdl_update()
{
	SDL_Flip(screen);
}

void sdl_lock()
{
	if (SDL_MUSTLOCK(screen) && (SDL_LockSurface(screen) < 0))
		LOG_W("Couldn't lock surface: %s\n", SDL_GetError());
}

void sdl_unlock()
{
	if (SDL_MUSTLOCK(screen))
		SDL_UnlockSurface(screen);
}

struct color sdl_get_pixel(int x, int y)
{
	int bpp = screen->format->BytesPerPixel;
	uint8_t *p;
	uint32_t pixel;
	struct color color;

	/* Apply scaling factor to coordinates and get pixel pointer */
	x *= scale_factor;
	y *= scale_factor;
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

void sdl_set_pixel(int x, int y, struct color color)
{
	uint32_t pixel;
	int bpp = screen->format->BytesPerPixel;
	uint8_t *p1;
	uint8_t *p2;
	int i;
	int j;

	/* Map color */
	pixel = SDL_MapRGB(screen->format, color.r, color.g, color.b);

	/* Apply scaling factor to coordinates */
	x *= scale_factor;
	y *= scale_factor;

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
	for (i = x; i < x + scale_factor; i++)
		for (j = y; j < y + scale_factor; j++) {
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

void sdl_deinit()
{
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

VIDEO_START(sdl)
	.input = "sdl",
	.init = sdl_init,
	.get_window = sdl_get_window,
	.update = sdl_update,
	.lock = sdl_lock,
	.unlock = sdl_unlock,
	.get_pixel = sdl_get_pixel,
	.set_pixel = sdl_set_pixel,
	.deinit = sdl_deinit
VIDEO_END

