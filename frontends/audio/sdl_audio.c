#include <stdio.h>
#include <stdbool.h>
#include <SDL.h>
#include <audio.h>
#include <log.h>
#include <util.h>

struct audio_data {
	void (*callback)(audio_data_t *data, void *buffer, int len);
	void *priv_data;
};

static bool sdl_init(struct audio_frontend *fe, struct audio_specs *specs);
static void sdl_mix(void *userdata, Uint8 *stream, int len);
static void sdl_start(struct audio_frontend *fe);
static void sdl_stop(struct audio_frontend *fe);
static void sdl_deinit(struct audio_frontend *fe);

bool sdl_init(struct audio_frontend *fe, struct audio_specs *specs)
{
	struct audio_data *audio_data;
	SDL_AudioSpec fmt;

	/* Initialize audio sub-system */
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		LOG_E("Error initializing SDL audio: %s\n", SDL_GetError());
		return false;
	}

	/* Set audio specs */
	fmt.freq = specs->freq;
	switch (specs->format) {
	case AUDIO_FORMAT_U8:
		fmt.format = AUDIO_U8;
		break;
	case AUDIO_FORMAT_S8:
		fmt.format = AUDIO_S8;
		break;
	case AUDIO_FORMAT_U16:
		fmt.format = AUDIO_U16;
		break;
	case AUDIO_FORMAT_S16:
		fmt.format = AUDIO_S16;
		break;
	default:
		LOG_E("Unknown audio format: %u\n", (int)specs->format);
		SDL_AudioQuit();
		return false;
	}
	fmt.channels = specs->channels;
	fmt.samples = specs->samples;
	fmt.callback = sdl_mix;
	fmt.userdata = fe;

	/* Open the audio device */
	if (SDL_OpenAudio(&fmt, NULL) < 0) {
		LOG_E("Unable to open audio: %s\n", SDL_GetError());
		SDL_AudioQuit();
		return false;
	}

	/* Save audio data */
	audio_data = malloc(sizeof(struct audio_data));
	audio_data->callback = specs->mix;
	audio_data->priv_data = specs->data;
	fe->priv_data = audio_data;

	return true;
}

void sdl_mix(void *userdata, Uint8 *stream, int len)
{
	struct audio_frontend *fe = userdata;
	struct audio_data *audio_data = fe->priv_data;
	audio_data->callback(audio_data->priv_data, stream, len);
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
	free(fe->priv_data);
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

AUDIO_START(sdl)
	.init = sdl_init,
	.start = sdl_start,
	.stop = sdl_stop,
	.deinit = sdl_deinit
AUDIO_END

