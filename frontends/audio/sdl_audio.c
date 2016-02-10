#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <audio.h>
#include <log.h>
#include <util.h>

#define AUDIO_LATENCY 50

typedef void (*sdl_callback)(void *userdata, Uint8 *stream, int len);

struct resample_data {
	uint64_t input_len;
	uint64_t output_len;
	uint64_t count;
	uint64_t left;
	uint64_t right;
};

struct audio_data {
	uint8_t *buffer;
	int buffer_size;
	enum audio_format format;
	int fmt_size;
	int num_channels;
	int freq;
	int head;
	int tail;
	int count;
	struct resample_data resample_data;
};

static bool sdl_init(struct audio_frontend *fe, struct audio_specs *specs);
static void sdl_enqueue(struct audio_frontend *fe, void *buffer, int count);
static void sdl_start(struct audio_frontend *fe);
static void sdl_stop(struct audio_frontend *fe);
static void sdl_deinit(struct audio_frontend *fe);
static void get_value(struct audio_data *data, void **buffer, uint64_t *v);
static void push(struct audio_data *data, uint8_t *buffer, int len);
static void pull(struct audio_data *data, void *buffer, int len);
static uint32_t gcd(uint32_t a, uint32_t b);
static uint32_t lcm(uint32_t a, uint32_t b);

uint32_t gcd(uint32_t a, uint32_t b)
{
	uint32_t t;
	while (b) {
		t = b;
		b = a % b;
		a = t;
	}
	return a;
}

uint32_t lcm(uint32_t a, uint32_t b)
{
	return a * b / gcd(a, b);
}

void get_value(struct audio_data *data, void **buffer, uint64_t *v)
{
	uint8_t **buf_u8 = (uint8_t **)buffer;
	int8_t **buf_s8 = (int8_t **)buffer;
	uint16_t **buf_u16 = (uint16_t **)buffer;
	int16_t **buf_s16 = (int16_t **)buffer;

	/* Get value based on format */
	switch (data->format) {
	case AUDIO_FORMAT_U8:
		*v = *(*buf_u8)++;
		break;
	case AUDIO_FORMAT_S8:
		*v = *(*buf_s8)++;
		break;
	case AUDIO_FORMAT_U16:
		*v = *(*buf_u16)++;
		break;
	case AUDIO_FORMAT_S16:
		*v = *(*buf_s16)++;
		break;
	}
}

void push(struct audio_data *data, uint8_t *buffer, int len)
{
	int len1 = len;
	int len2 = 0;

	/* Lock access */
	SDL_LockAudio();

	/* Handle overrun */
	if (data->count + len > data->buffer_size) {
		LOG_D("Audio overrun!\n");
		len = data->buffer_size - data->count;
		len1 = len;
	}

	/* Handle wrapping */
	if (data->head + len > data->buffer_size) {
		len1 = data->buffer_size - data->head;
		len2 = len - len1;
	}

	/* Copy buffer */
	memcpy(&data->buffer[data->head], buffer, len1);
	memcpy(data->buffer, &buffer[len1], len2);

	/* Move head and update count */
	data->head = (data->head + len) % data->buffer_size;
	data->count += len;

	/* Unlock access */
	SDL_UnlockAudio();
}

void pull(struct audio_data *data, void *buffer, int len)
{
	uint8_t *buf = buffer;
	int len1 = len;
	int len2 = 0;

	/* Lock access */
	SDL_LockAudio();

	/* Empty buffer first */
	SDL_memset(buffer, 0, len);

	/* Handle underrun */
	if (data->count < len) {
		LOG_D("Audio underrun!\n");
		len = data->count;
		len1 = len;
	}

	/* Handle wrapping */
	if (data->tail + len > data->buffer_size) {
		len1 = data->buffer_size - data->tail;
		len2 = len - len1;
	}

	/* Copy buffer */
	memcpy(buf, &data->buffer[data->tail], len1);
	memcpy(&buf[len1], data->buffer, len2);

	/* Move tail and update count */
	data->tail = (data->tail + len) % data->buffer_size;
	data->count -= len;

	/* Unlock access */
	SDL_UnlockAudio();
}

bool sdl_init(struct audio_frontend *fe, struct audio_specs *specs)
{
	struct audio_data *audio_data;
	SDL_AudioSpec desired;
	Uint16 fmt;
	int fmt_size;
	int samples;
	uint64_t v;

	/* Initialize audio sub-system */
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		LOG_E("Error initializing SDL audio: %s\n", SDL_GetError());
		return false;
	}

	/* Allocate audio data */
	audio_data = calloc(1, sizeof(struct audio_data));
	fe->priv_data = audio_data;

	/* Compute buffer lengths based on input and output frequencies */
	v = lcm(specs->freq, specs->sampling_rate);
	audio_data->resample_data.input_len = v / specs->freq;
	audio_data->resample_data.output_len = v / specs->sampling_rate;

	/* Initialize remaining resampling data */
	audio_data->resample_data.count = 0;
	audio_data->resample_data.left = 0;
	audio_data->resample_data.right = 0;

	/* Set number of samples based on desired output frequency */
	switch (specs->sampling_rate) {
	case 11025:
		samples = 512;
		break;
	case 22050:
		samples = 1024;
		break;
	case 44100:
	case 48000:
	default:
		samples = 2048;
		break;
	}

	/* Set desired format */
	switch (specs->format) {
	case AUDIO_FORMAT_U8:
		fmt = AUDIO_U8;
		fmt_size = sizeof(uint8_t);
		break;
	case AUDIO_FORMAT_S8:
		fmt = AUDIO_S8;
		fmt_size = sizeof(int8_t);
		break;
	case AUDIO_FORMAT_U16:
		fmt = AUDIO_U16;
		fmt_size = sizeof(uint16_t);
		break;
	case AUDIO_FORMAT_S16:
	default:
		fmt = AUDIO_S16;
		fmt_size = sizeof(int16_t);
		break;
	}

	/* Set audio specs */
	desired.freq = specs->sampling_rate;
	desired.format = fmt;
	desired.channels = specs->channels;
	desired.samples = samples;
	desired.callback = (sdl_callback)pull;
	desired.userdata = audio_data;

	/* Open the audio device */
	if (SDL_OpenAudio(&desired, NULL) < 0) {
		LOG_E("Unable to open audio: %s\n", SDL_GetError());
		free(audio_data);
		SDL_AudioQuit();
		return false;
	}

	/* Compute buffer size (based on desired specs and latency) */
	audio_data->buffer_size = specs->sampling_rate;
	audio_data->buffer_size *= specs->channels * fmt_size;
	audio_data->buffer_size *= AUDIO_LATENCY / 1000.0f;
	audio_data->buffer_size *= 2;
	LOG_D("Computed audio buffer size: %ub\n", audio_data->buffer_size);

	/* Initialize audio data */
	audio_data->buffer = calloc(audio_data->buffer_size, sizeof(uint8_t));
	audio_data->format = specs->format;
	audio_data->fmt_size = fmt_size;
	audio_data->num_channels = specs->channels;
	audio_data->freq = specs->freq;
	audio_data->head = 0;
	audio_data->tail = 0;
	audio_data->count = 0;

	return true;
}

void sdl_enqueue(struct audio_frontend *fe, void *buffer, int count)
{
	struct audio_data *data = fe->priv_data;
	struct resample_data *resample_data = &data->resample_data;
	bool stereo = (data->num_channels == 2);
	uint64_t v_l = 0;
	uint64_t v_r = 0;
	uint64_t l;
	uint64_t d;
	int i;

	for (i = 0; i < count; i++) {
		/* Reset length */
		l = resample_data->input_len;

		/* Get left value (and right value if available) */
		get_value(data, &buffer, &v_l);
		if (stereo)
			get_value(data, &buffer, &v_r);

		/* Check if value needs to be pushed to output buffer */
		if (resample_data->count + l >= resample_data->output_len) {
			/* Set buffer length to use */
			l = resample_data->output_len - resample_data->count;

			/* Compute final left value and push it */
			resample_data->left += l * v_l;
			d = (resample_data->left / resample_data->output_len);
			push(data, (uint8_t *)&d, data->fmt_size);

			/* Compute final right value and push it */
			resample_data->right += l * v_r;
			d = (resample_data->right / resample_data->output_len);
			if (stereo)
				push(data, (uint8_t *)&d, data->fmt_size);

			/* Reset resampling data */
			resample_data->count = 0;
			resample_data->left = 0;
			resample_data->right = 0;

			/* Set remaining buffer length */
			l = resample_data->input_len - l;
		}

		/* Update buffer count and value */
		resample_data->count += l;
		resample_data->left += v_l * l;
		resample_data->right += v_r * l;
	}
}

void sdl_start(struct audio_frontend *UNUSED(fe))
{
	SDL_PauseAudio(0);
}

void sdl_stop(struct audio_frontend *UNUSED(fe))
{
	SDL_PauseAudio(1);
}

void sdl_deinit(struct audio_frontend *fe)
{
	struct audio_data *audio_data = fe->priv_data;
	free(audio_data->buffer);
	free(audio_data);
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

AUDIO_START(sdl)
	.init = sdl_init,
	.enqueue = sdl_enqueue,
	.start = sdl_start,
	.stop = sdl_stop,
	.deinit = sdl_deinit
AUDIO_END

