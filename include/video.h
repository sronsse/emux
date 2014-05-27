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

typedef void window_t;
typedef void video_priv_data_t;

struct video_specs {
	int width;
	int height;
	float fps;
	int scale;
};

struct color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct video_frontend {
	char *name;
	char *input;
	video_priv_data_t *priv_data;
	window_t *(*init)(struct video_frontend *fe, struct video_specs *vs);
	void (*update)(struct video_frontend *fe);
	void (*lock)(struct video_frontend *fe);
	void (*unlock)(struct video_frontend *fe);
	struct color (*get_p)(struct video_frontend *fe, int x, int y);
	void (*set_p)(struct video_frontend *fe, int x, int y, struct color c);
	void (*deinit)(struct video_frontend *fe);
};

bool video_init(struct video_specs *vs);
void video_update();
void video_lock();
void video_unlock();
struct color video_get_pixel(int x, int y);
void video_set_pixel(int x, int y, struct color color);
void video_deinit();

extern struct list_link *video_frontends;

#endif

