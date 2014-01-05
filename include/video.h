#ifndef _VIDEO_H
#define _VIDEO_H

#include <stdbool.h>
#include <stdint.h>
#include <list.h>

#define VIDEO_START(_name) \
	static struct video_frontend _video_frontend = { \
		.name = #_name,
#define VIDEO_END \
	}; \
	__attribute__((constructor)) static void _register() \
	{ \
		list_insert(&video_frontends, &_video_frontend); \
	} \
	__attribute__((destructor)) static void _unregister() \
	{ \
		list_remove(&video_frontends, &_video_frontend); \
	}

typedef void video_window_t;

struct color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct video_frontend {
	char *name;
	char *input;
	bool (*init)(int width, int height, int scale);
	video_window_t *(*get_window)();
	void (*update)();
	void (*lock)();
	void (*unlock)();
	struct color (*get_pixel)(int x, int y);
	void (*set_pixel)(int x, int y, struct color color);
	void (*deinit)();
};

bool video_init(int width, int height);
video_window_t *video_get_window();
void video_update();
void video_lock();
void video_unlock();
struct color video_get_pixel(int x, int y);
void video_set_pixel(int x, int y, struct color color);
void video_deinit();

extern struct list_link *video_frontends;

#endif

