#include <stdio.h>
#include <stdbool.h>
#include <SDL.h>
#include <video.h>

static video_window_t *sdl_init(int width, int height, int scale);
static void sdl_update();
static uint32_t sdl_map_rgb(uint8_t r, uint8_t g, uint8_t b);
static uint32_t sdl_get_pixel(int x, int y);
static void sdl_set_pixel(int x, int y, uint32_t pixel);
static void sdl_deinit();

static SDL_Surface *screen;
static int scale_factor;

video_window_t *sdl_init(int width, int height, int scale)
{
	Uint32 flags = SDL_SWSURFACE;

	/* Initialize video sub-system */
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "Error initializing SDL video: %s\n",
			SDL_GetError());
		return NULL;
	}

	/* Set window position and title */
	SDL_putenv("SDL_VIDEO_CENTERED=center");
	SDL_WM_SetCaption("emux", NULL);

	/* Create main video surface */
	screen = SDL_SetVideoMode(width * scale, height * scale, 0, flags);
	if (!screen) {
		fprintf(stderr, "Error create video surface! %s\n",
			SDL_GetError());
		SDL_VideoQuit();
		return NULL;
	}

	/* Save scaling factor */
	scale_factor = scale;

	return screen;
}

void sdl_update()
{
	SDL_Flip(screen);
}

void sdl_lock()
{
	if (SDL_MUSTLOCK(screen) && (SDL_LockSurface(screen) < 0))
		fprintf(stderr, "Can't lock surface: %s\n", SDL_GetError());
}

void sdl_unlock()
{
	if (SDL_MUSTLOCK(screen))
		SDL_UnlockSurface(screen);
}

uint32_t sdl_map_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	return SDL_MapRGB(screen->format, r, g, b);
}

uint32_t sdl_get_pixel(int x, int y)
{
	int bpp = screen->format->BytesPerPixel;
	uint8_t *p;
	uint32_t result;

	/* Apply scaling factor to coordinates and get pixel pointer */
	x *= scale_factor;
	y *= scale_factor;
	p = (uint8_t *)screen->pixels + y * screen->pitch + x * bpp;

	/* Read pixel */
	switch (bpp) {
	case 1:
		result = *p;
		break;
	case 2:
		result = *(uint16_t *)p;
		break;
	case 3:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
			result = (p[0] << 16) | (p[1] << 8) | p[2];
		else
			result = p[0] | (p[1] << 8) | (p[2] << 16);
		break;
	case 4:
		result = *(uint32_t *)p;
		break;
	default:
		result = 0;
		break;
	}

	return result;
}

void sdl_set_pixel(int x, int y, uint32_t pixel)
{
	int bpp = screen->format->BytesPerPixel;
	uint8_t *p;
	int i;
	int j;

	/* Apply scaling factor to coordinates */
	x *= scale_factor;
	y *= scale_factor;

	/* Write square of pixels depending on scaling factor */
	for (i = x; i < x + scale_factor; i++)
		for (j = y; j < y + scale_factor; j++) {
			/* Compute pixel pointer */
			p = (uint8_t *)screen->pixels + j * screen->pitch +
				i * bpp;

			/* Write pixel */
			switch (bpp) {
			case 1:
				*p = pixel;
				break;
			case 2:
				*(uint16_t *)p = pixel;
				break;
			case 3:
				if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
					p[0] = (pixel >> 16) & 0xFF;
					p[1] = (pixel >> 8) & 0xFF;
					p[2] = pixel & 0xFF;
				} else {
					p[0] = pixel & 0xFF;
					p[1] = (pixel >> 8) & 0xFF;
					p[2] = (pixel >> 16) & 0xFF;
				}
				break;
			case 4:
				*(uint32_t *)p = pixel;
				break;
			default:
				break;
			}
		}
}

void sdl_deinit()
{
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

VIDEO_START(sdl)
	.init = sdl_init,
	.update = sdl_update,
	.lock = sdl_lock,
	.unlock = sdl_unlock,
	.map_rgb = sdl_map_rgb,
	.get_pixel = sdl_get_pixel,
	.set_pixel = sdl_set_pixel,
	.deinit = sdl_deinit
VIDEO_END

