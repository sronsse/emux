#ifndef _AUDIO_H
#define _AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#define AUDIO_START(_name) \
	static struct audio_frontend audio_##_name \
		__attribute__(( \
			__used__, \
			__section__("audio_frontends"), \
			__aligned__(__alignof__(struct audio_frontend)))) = { \
		.name = #_name,
#define AUDIO_END \
	};

typedef void audio_data_t;

enum audio_format {
	AUDIO_FORMAT_U8,
	AUDIO_FORMAT_S8,
	AUDIO_FORMAT_U16,
	AUDIO_FORMAT_S16
};

struct audio_specs {
	int freq;
	enum audio_format format;
	int channels;
	int samples;
	void (*mix)(audio_data_t *data, void *buffer, int len);
	audio_data_t *data;
};

struct audio_frontend {
	char *name;
	bool (*init)(struct audio_specs *specs);
	void (*start)();
	void (*stop)();
	void (*deinit)();
};

bool audio_init(struct audio_specs *specs);
void audio_start();
void audio_stop();
void audio_deinit();

#endif

