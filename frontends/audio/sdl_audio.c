#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <audio.h>
#include <log.h>
#include <util.h>

#define LATENCY_MS_MAX	100
#define NUM_BUFFERS	4

typedef void (*sdl_callback)(void *userdata, Uint8 *stream, int len);

struct audio_data {
	uint8_t *buffer;
	int buffer_size;
	int head;
	int tail;
	int count;
};

static bool sdl_init(struct audio_frontend *fe, int sampling_rate);
static void sdl_enqueue(struct audio_frontend *fe, int16_t left, int16_t right);
static void sdl_dequeue(struct audio_data *data, void *buffer, int len);
static void sdl_start(struct audio_frontend *fe);
static void sdl_stop(struct audio_frontend *fe);
static void sdl_deinit(struct audio_frontend *fe);

bool sdl_init(struct audio_frontend *fe, int sampling_rate)
{
	struct audio_data *audio_data;
	SDL_AudioSpec desired;
	int samples;

	/* Initialize audio sub-system */
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		LOG_E("Error initializing SDL audio: %s\n", SDL_GetError());
		return false;
	}

	/* Allocate audio data */
	audio_data = calloc(1, sizeof(struct audio_data));
	fe->priv_data = audio_data;

	/* Set number of samples based on desired output frequency */
	switch (sampling_rate) {
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

	/* Set audio specs (16-bit signed, stereo) */
	desired.freq = sampling_rate;
	desired.format = AUDIO_S16;
	desired.channels = 2;
	desired.samples = samples;
	desired.callback = (sdl_callback)sdl_dequeue;
	desired.userdata = audio_data;

	/* Open the audio device */
	if (SDL_OpenAudio(&desired, NULL) < 0) {
		LOG_E("Unable to open audio: %s\n", SDL_GetError());
		free(audio_data);
		SDL_AudioQuit();
		return false;
	}

	/* Compute buffer size (based on desired specs and latency) */
	audio_data->buffer_size = sampling_rate;
	audio_data->buffer_size *= 2 * sizeof(int16_t);
	audio_data->buffer_size *= LATENCY_MS_MAX / 1000.0f;
	audio_data->buffer_size *= NUM_BUFFERS;
	LOG_D("Computed audio buffer size: %ub\n", audio_data->buffer_size);

	/* Initialize audio data */
	audio_data->buffer = calloc(audio_data->buffer_size, sizeof(uint8_t));
	audio_data->head = 0;
	audio_data->tail = 0;
	audio_data->count = 0;

	return true;
}

void sdl_enqueue(struct audio_frontend *fe, int16_t left, int16_t right)
{
	struct audio_data *data = fe->priv_data;
	int16_t *buffer = (int16_t *)data->buffer;

	/* Lock access */
	SDL_LockAudio();

	/* Handle overrun */
	if (data->count == data->buffer_size)
		LOG_D("Audio overrun!\n");

	/* Copy data */
	buffer[data->head / sizeof(int16_t)] = left;
	buffer[(data->head + sizeof(int16_t)) / sizeof(int16_t)] = right;

	/* Update head */
	data->head += 2 * sizeof(int16_t);
	if (data->head == data->buffer_size)
		data->head = 0;

	/* Update count */
	data->count += 2 * sizeof(int16_t);

	/* Unlock access */
	SDL_UnlockAudio();
}

void sdl_dequeue(struct audio_data *data, void *buffer, int len)
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

