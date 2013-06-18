#include <stdio.h>
#include <stdbool.h>
#include <SDL.h>
#include <video.h>

static video_window_t *sdl_init(int width, int height);
static video_surface_t *sdl_create_surface(int width, int height);
static void sdl_free_surface(video_surface_t *s);
static void sdl_blit_surface(video_surface_t *s);
static void sdl_update();
static uint32_t sdl_map_rgb(video_surface_t *s, uint8_t r, uint8_t g,
	uint8_t b);
static uint32_t sdl_get_pixel(video_surface_t *s, int x, int y);
static void sdl_set_pixel(video_surface_t *s, int x, int y, uint32_t pixel);
static void sdl_deinit();

static SDL_Surface *screen;

video_window_t *sdl_init(int width, int height)
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
	screen = SDL_SetVideoMode(width, height, 0, flags);
	if (!screen) {
		fprintf(stderr, "Error create video surface! %s\n",
			SDL_GetError());
		SDL_VideoQuit();
		return NULL;
	}

	return screen;
}

video_surface_t *sdl_create_surface(int width, int height)
{
	SDL_Surface *surface;
	SDL_PixelFormat *fmt;

	/* Create surface using screen format */
	fmt = screen->format;
	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height,
		fmt->BitsPerPixel, fmt->Rmask, fmt->Gmask, fmt->Bmask,
		fmt->Amask);
	if (!surface)
		fprintf(stderr, "Failed creating video surface: %s\n",
			SDL_GetError());
	return surface;
}

void sdl_free_surface(video_surface_t *s)
{
	SDL_FreeSurface((SDL_Surface *)s);
}

void sdl_blit_surface(video_surface_t *s)
{
	SDL_Surface *surface = s;
	int x, y, i, j;
	uint32_t pixel;

	for (i = 0; i < screen->w; i++)
		for (j = 0; j < screen->h; j++) {
			x = i * surface->w / screen->w;
			y = j * surface->h / screen->h;
			pixel = sdl_get_pixel(s, x, y);
			sdl_set_pixel(screen, i, j, pixel);
		}
}

void sdl_update()
{
	SDL_Flip(screen);
}

uint32_t sdl_map_rgb(video_surface_t *s, uint8_t r, uint8_t g, uint8_t b)
{
	SDL_Surface *surface = (SDL_Surface *)s;
	return SDL_MapRGB(surface->format, r, g, b);
}

uint32_t sdl_get_pixel(video_surface_t *s, int x, int y)
{
	SDL_Surface *surface = (SDL_Surface *)s;
	int bpp = surface->format->BytesPerPixel;
	uint8_t *p = (uint8_t *)surface->pixels + y * surface->pitch + x * bpp;

	if (SDL_MUSTLOCK(surface) && (SDL_LockSurface(surface) < 0)) {
		fprintf(stderr, "Can't lock surface: %s\n", SDL_GetError());
		return 0;
	}

	switch (bpp) {
	case 1:
		return *p;
	case 2:
		return *(uint16_t *)p;
	case 3:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
			return (p[0] << 16) | (p[1] << 8) | p[2];
		else
			return p[0] | (p[1] << 8) | (p[2] << 16);
	case 4:
		return *(uint32_t *)p;
	default:
		return 0;
	}

	if (SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);
}

void sdl_set_pixel(video_surface_t *s, int x, int y, uint32_t pixel)
{
	SDL_Surface *surface = (SDL_Surface *)s;
	int bpp = surface->format->BytesPerPixel;
	uint8_t *p = (uint8_t *)surface->pixels + y * surface->pitch + x * bpp;

	if (SDL_MUSTLOCK(surface) && (SDL_LockSurface(surface) < 0)) {
		fprintf(stderr, "Can't lock surface: %s\n", SDL_GetError());
		return;
	}

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

	if (SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);
}

void sdl_deinit()
{
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

VIDEO_START(sdl)
	.init = sdl_init,
	.create_surface = sdl_create_surface,
	.free_surface = sdl_free_surface,
	.blit_surface = sdl_blit_surface,
	.update = sdl_update,
	.map_rgb = sdl_map_rgb,
	.get_pixel = sdl_get_pixel,
	.set_pixel = sdl_set_pixel,
	.deinit = sdl_deinit
VIDEO_END

