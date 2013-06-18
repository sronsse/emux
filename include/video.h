#ifndef _VIDEO_H
#define _VIDEO_H

#include <stdbool.h>
#include <stdint.h>

#define VIDEO_START(_name) \
	static struct video_frontend video_##_name \
		__attribute__(( \
			__used__, \
			__section__("video_frontends"), \
			__aligned__(__alignof__(struct video_frontend)))) = { \
		.name = #_name,
#define VIDEO_END \
	};

typedef void video_window_t;
typedef void video_surface_t;

struct video_frontend {
	char *name;
	video_window_t *(*init)(int width, int height);
	video_surface_t *(*create_surface)(int width, int height);
	void (*free_surface)(video_surface_t *s);
	void (*blit_surface)(video_surface_t *s);
	void (*update)();
	uint32_t (*map_rgb)(video_surface_t *s, uint8_t r, uint8_t g,
		uint8_t b);
	uint32_t (*get_pixel)(video_surface_t *s, int x, int y);
	void (*set_pixel)(video_surface_t *s, int x, int y, uint32_t pixel);
	void (*deinit)();
};

bool video_init(int width, int height);
video_surface_t *video_create_surface(int width, int height);
void video_free_surface(video_surface_t *s);
void video_blit_surface(video_surface_t *s);
void video_update();
uint32_t video_map_rgb(video_surface_t *s, uint8_t r, uint8_t g, uint8_t b);
uint32_t video_get_pixel(video_surface_t *s, int x, int y);
void video_set_pixel(video_surface_t *s, int x, int y, uint32_t pixel);
void video_deinit();

#endif

