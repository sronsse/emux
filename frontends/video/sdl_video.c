#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <log.h>
#include <util.h>
#include <video.h>

#define BIT_DEPTH	32

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define RMASK		0xFF000000
#define GMASK		0x00FF0000
#define BMASK		0x0000FF00
#define AMASK		0x000000FF
#else
#define RMASK		0x000000FF
#define GMASK		0x0000FF00
#define BMASK		0x00FF0000
#define AMASK		0xFF000000
#endif

struct sdl_data {
	SDL_Window *window;
	SDL_Surface *screen;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	int width;
	int height;
	int scale;
};

static window_t *sdl_init(struct video_frontend *fe, struct video_specs *vs);
static void sdl_update(struct video_frontend *fe);
static void sdl_lock(struct video_frontend *fe);
static void sdl_unlock(struct video_frontend *fe);
static window_t *sdl_set_size(struct video_frontend *fe, int w, int h);
static struct color sdl_get_p(struct video_frontend *fe, int x, int y);
static void sdl_set_p(struct video_frontend *fe, int x, int y, struct color c);
static void sdl_deinit(struct video_frontend *fe);

window_t *sdl_init(struct video_frontend *fe, struct video_specs *vs)
{
	struct sdl_data *data;
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Surface *screen;
	SDL_Texture *texture;
	int w = vs->width * vs->scale;
	int h = vs->height * vs->scale;

	/* Initialize video sub-system */
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		LOG_E("Error initializing SDL video: %s\n", SDL_GetError());
		return NULL;
	}

	/* Set window position and title */
	window = SDL_CreateWindow("emux",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		w,
		h,
		SDL_WINDOW_RESIZABLE);
	if (!window) {
		LOG_E("Error creating window: %s\n", SDL_GetError());
		SDL_VideoQuit();
		return NULL;
	}

	/* Create renderer */
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (renderer == NULL) {
		LOG_E("Error creating renderer: %s\n", SDL_GetError());
		SDL_VideoQuit();
		return NULL;
	}

	/* Create screen */
	screen = SDL_CreateRGBSurface(0,
		w,
		h,
		BIT_DEPTH,
		RMASK,
		GMASK,
		BMASK,
		AMASK);
	if (!screen) {
		LOG_E("Error creating surface: %s\n", SDL_GetError());
		SDL_VideoQuit();
		return NULL;
	}

	/* Create texture based on screen format */
	texture = SDL_CreateTexture(renderer,
		screen->format->format,
		SDL_TEXTUREACCESS_STREAMING,
		w,
		h);
	if (!texture) {
		LOG_E("Error creating texture: %s\n", SDL_GetError());
		SDL_VideoQuit();
		return NULL;
	}

	/* Create and fill private data */
	data = calloc(1, sizeof(struct sdl_data));
	data->window = window;
	data->screen = screen;
	data->renderer = renderer;
	data->texture = texture;
	data->width = vs->width;
	data->height = vs->height;
	data->scale = vs->scale;
	fe->priv_data = data;

	return screen;
}

void sdl_update(struct video_frontend *fe)
{
	struct sdl_data *data = fe->priv_data;
	SDL_Surface *screen = data->screen;
	SDL_Renderer *renderer = data->renderer;
	SDL_Texture *texture = data->texture;

	SDL_UpdateTexture(texture, NULL, screen->pixels, screen->pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
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

window_t *sdl_set_size(struct video_frontend *fe, int w, int h)
{
	struct sdl_data *data = fe->priv_data;

	/* Save dimensions */
	data->width = w;
	data->height = h;

	/* Adapt dimensions based on scale */
	w *= data->scale;
	h *= data->scale;

	/* Free existing screen surface and texture */
	SDL_FreeSurface(data->screen);
	SDL_DestroyTexture(data->texture);

	/* Update window size */
	SDL_SetWindowSize(data->window, w, h);

	/* Re-create screen surface */
	data->screen = SDL_CreateRGBSurface(0,
		w,
		h,
		BIT_DEPTH,
		RMASK,
		GMASK,
		BMASK,
		AMASK);

	/* Re-create texture */
	data->texture = SDL_CreateTexture(data->renderer,
		data->screen->format->format,
		SDL_TEXTUREACCESS_STREAMING,
		w,
		h);

	return data->screen;
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
	memcpy(&pixel, p, bpp);

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
	memcpy(p1, &pixel, bpp);

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
	struct sdl_data *data = fe->priv_data;

	/* Free SDL resources */
	SDL_DestroyTexture(data->texture);
	SDL_DestroyRenderer(data->renderer);
	SDL_FreeSurface(data->screen);

	/* Free subsystem and private data */
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	free(fe->priv_data);
}

VIDEO_START(sdl)
	.input = "sdl",
	.init = sdl_init,
	.update = sdl_update,
	.lock = sdl_lock,
	.unlock = sdl_unlock,
	.set_size = sdl_set_size,
	.get_p = sdl_get_p,
	.set_p = sdl_set_p,
	.deinit = sdl_deinit
VIDEO_END
