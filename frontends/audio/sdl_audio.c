#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <audio.h>
#include <log.h>
#include <util.h>

#define AUDIO_LATENCY_MS_MAX 100

typedef void (*sdl_callback)(void *userdata, Uint8 *stream, int len);

struct resample_data {
	float mul;
	float step;
	int count;
	int left;
	int right;
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
static int get_value(struct audio_data *data, void **buffer);
static void push(struct audio_data *data, uint8_t b);
static void pull(struct audio_data *data, void *buffer, int len);

int get_value(struct audio_data *data, void **buffer)
{
	int v = 0;

	/* Get value based on format */
	switch (data->format) {
	case AUDIO_FORMAT_U8:
		v = *((uint8_t *)*buffer);
		*buffer += sizeof(uint8_t);
		break;
	case AUDIO_FORMAT_S8:
		v = *((int8_t *)*buffer);
		*buffer += sizeof(int8_t);
		break;
	case AUDIO_FORMAT_U16:
		v = *((uint16_t *)*buffer);
		*buffer += sizeof(uint16_t);
		break;
	case AUDIO_FORMAT_S16:
		v = *((int16_t *)*buffer);
		*buffer += sizeof(int16_t);
		break;
	}

	/* Return value */
	return v;
}

void push(struct audio_data *data, uint8_t b)
{
	/* Lock access */
	SDL_LockAudio();

	/* Handle overrun */
	if (data->count == data->buffer_size)
		LOG_D("Audio overrun!\n");

	/* Copy data */
	data->buffer[data->head] = b;

	/* Move head and update count */
	data->head = (data->head + 1) % data->buffer_size;
	data->count++;

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

	/* Initialize audio sub-system */
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		LOG_E("Error initializing SDL audio: %s\n", SDL_GetError());
		return false;
	}

	/* Allocate audio data */
	audio_data = calloc(1, sizeof(struct audio_data));
	fe->priv_data = audio_data;

	/* Compute mutiplier factor between output and input frequencies */
	audio_data->resample_data.mul = specs->sampling_rate / specs->freq;

	/* Initialize remaining resampling data */
	audio_data->resample_data.step = 0.0f;
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
	audio_data->buffer_size *= AUDIO_LATENCY_MS_MAX / 1000.0f;
	audio_data->buffer_size *= 2;
	LOG_E("Computed audio buffer size: %ub\n", audio_data->buffer_size);

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
	struct audio_data *audio_data = fe->priv_data;
	struct resample_data *resample_data = &audio_data->resample_data;
	bool stereo = (audio_data->num_channels == 2);
	float prev_step;
	uint8_t d;
	int i;

	/* Parse all input buffer samples */
	for (i = 0; i < count; i++) {
		/* Get left (or mono) value */
		resample_data->left += get_value(audio_data, &buffer);

		/* Get right value if needed */
		if (stereo)
			resample_data->right += get_value(audio_data, &buffer);

		/* Increment resample data count */
		resample_data->count++;

		/* Compute next step and continue if no output is generated */
		prev_step = resample_data->step;
		resample_data->step = prev_step + resample_data->mul;
		if ((int)prev_step == (int)resample_data->step)
			continue;

		/* Compute final left (or mono) value and push it */
		d = resample_data->left / resample_data->count;
		push(audio_data, d);

		/* Compute final right value and push it if needed */
		if (stereo) {
			d = (resample_data->right / resample_data->count);
			push(audio_data, d);
		}

		/* Reset resample data */
		resample_data->step -= 1.0f;
		resample_data->count = 0;
		resample_data->left = 0;
		resample_data->right = 0;
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

