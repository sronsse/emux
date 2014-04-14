#include <stdio.h>
#include <stdbool.h>
#include <SDL.h>
#include <audio.h>
#include <log.h>
#include <util.h>

typedef void (*sdl_callback)(void *userdata, Uint8 *stream, int len);

struct audio_data {
	uint8_t *buffer;
	int buffer_size;
	int fmt_size;
	int num_channels;
	int freq;
	int head;
	int tail;
	int count;
	void *priv_data;
};

static bool sdl_init(struct audio_frontend *fe, struct audio_specs *specs);
static void sdl_enqueue(struct audio_frontend *fe, uint8_t *buffer, int len);
static void sdl_dequeue(struct audio_frontend *fe, uint8_t *buffer, int len);
static void sdl_start(struct audio_frontend *fe);
static void sdl_stop(struct audio_frontend *fe);
static void sdl_deinit(struct audio_frontend *fe);

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
	audio_data = malloc(sizeof(struct audio_data));
	fe->priv_data = audio_data;

	/* Set number of samples based on desired frequency */
	switch (specs->freq) {
	case 11025:
		samples = 512;
		break;
	case 22050:
		samples = 1024;
		break;
	case 44100:
		samples = 2048;
		break;
	case 48000:
		samples = 2048;
		break;
	default:
		LOG_E("Audio frequency %u not supported!\n", specs->freq);
		free(audio_data);
		return false;
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
	desired.freq = specs->freq;
	desired.format = fmt;
	desired.channels = specs->channels;
	desired.samples = samples;
	desired.callback = (sdl_callback)sdl_dequeue;
	desired.userdata = fe;

	/* Open the audio device */
	if (SDL_OpenAudio(&desired, NULL) < 0) {
		LOG_E("Unable to open audio: %s\n", SDL_GetError());
		free(audio_data);
		SDL_AudioQuit();
		return false;
	}

	/* Compute buffer size (holding one second) */
	audio_data->buffer_size = specs->freq * specs->channels * fmt_size;
	LOG_D("Computed audio buffer size: %ub\n", audio_data->buffer_size);

	/* Initialize audio data */
	audio_data->buffer = malloc(audio_data->buffer_size);
	audio_data->fmt_size = fmt_size;
	audio_data->num_channels = specs->channels;
	audio_data->freq = specs->freq;
	audio_data->head = 0;
	audio_data->tail = 0;
	audio_data->count = 0;

	return true;
}

void sdl_enqueue(struct audio_frontend *fe, uint8_t *buffer, int len)
{
	struct audio_data *data = fe->priv_data;
	int len1 = len;
	int len2 = 0;

	/* Lock access */
	SDL_LockAudio();

	/* Handle overrun */
	if (data->count + len > data->buffer_size) {
		len = data->buffer_size - data->count;
		LOG_D("Audio overrun!\n");
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

void sdl_dequeue(struct audio_frontend *fe, uint8_t *buffer, int len)
{
	struct audio_data *data = fe->priv_data;
	int len1 = len;
	int len2 = 0;

	/* Lock access */
	SDL_LockAudio();

	/* Handle underrun */
	if (data->count < len) {
		SDL_UnlockAudio();
		LOG_D("Audio underrun!\n");
		return;
	}

	/* Handle wrapping */
	if (data->tail + len > data->buffer_size) {
		len1 = data->buffer_size - data->tail;
		len2 = len - len1;
	}

	/* Copy buffer */
	memcpy(buffer, &data->buffer[data->tail], len1);
	memcpy(&buffer[len1], data->buffer, len2);

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

