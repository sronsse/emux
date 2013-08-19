#ifndef _VIDEO_H
#define _VIDEO_H

#include <stdbool.h>
#include <stdint.h>

#define VIDEO_SECTION_NAME	"video_frontends"
#ifdef __APPLE__
#define VIDEO_SEGMENT_NAME	"__DATA"
#define VIDEO_SECTION_LABEL	VIDEO_SEGMENT_NAME ", " VIDEO_SECTION_NAME
#else
#define VIDEO_SECTION_LABEL	VIDEO_SECTION_NAME
#endif

#define VIDEO_START(_name) \
	static struct video_frontend video_##_name \
		__attribute__(( \
			__used__, \
			__section__(VIDEO_SECTION_LABEL), \
			__aligned__(__alignof__(struct video_frontend)))) = { \
		.name = #_name,
#define VIDEO_END \
	};

typedef void video_window_t;

struct color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct video_frontend {
	char *name;
	video_window_t *(*init)(int width, int height, int scale);
	void (*update)();
	void (*lock)();
	void (*unlock)();
	struct color (*get_pixel)(int x, int y);
	void (*set_pixel)(int x, int y, struct color color);
	void (*deinit)();
};

bool video_init(int width, int height);
void video_update();
void video_lock();
void video_unlock();
struct color video_get_pixel(int x, int y);
void video_set_pixel(int x, int y, struct color color);
void video_deinit();

#endif

