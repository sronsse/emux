#ifndef _AUDIO_H
#define _AUDIO_H

#include <stdbool.h>
#include <stdint.h>
#include <list.h>
#include <util.h>

#define AUDIO_START(_name) \
	static struct audio_frontend _audio_frontend = { \
		.name = #_name,
#define AUDIO_END \
	}; \
	static void _unregister(void) \
	{ \
		list_remove(&audio_frontends, &_audio_frontend); \
	} \
	INITIALIZER(_register) \
	{ \
		list_insert(&audio_frontends, &_audio_frontend); \
		atexit(_unregister); \
	}

typedef void audio_priv_data_t;

enum audio_format {
	AUDIO_FORMAT_U8,
	AUDIO_FORMAT_S8,
	AUDIO_FORMAT_U16,
	AUDIO_FORMAT_S16
};

struct audio_specs {
	float freq;
	enum audio_format format;
	int channels;
};

struct audio_frontend {
	char *name;
	audio_priv_data_t *priv_data;
	bool (*init)(struct audio_frontend *fe, int sampling_rate);
	void (*enqueue)(struct audio_frontend *fe, int16_t left, int16_t right);
	void (*start)(struct audio_frontend *fe);
	void (*stop)(struct audio_frontend *fe);
	void (*deinit)(struct audio_frontend *fe);
};

bool audio_init();
void audio_enqueue(void *buffer, int count);
void audio_start();
void audio_stop();
void audio_deinit();

extern struct list_link *audio_frontends;

#endif

