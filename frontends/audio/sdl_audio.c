#include <stdio.h>
#include <stdbool.h>
#include <SDL.h>
#include <audio.h>
#include <util.h>

struct audio_data {
	void (*callback)(audio_data_t *data, void *buffer, int len);
	void *priv_data;
};

static bool sdl_init(struct audio_specs *specs);
static void sdl_mix(void *unused, Uint8 *stream, int len);
static void sdl_start();
static void sdl_stop();
static void sdl_deinit();

static struct audio_data audio_data;

bool sdl_init(struct audio_specs *specs)
{
	SDL_AudioSpec fmt;

	/* Initialize audio sub-system */
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		fprintf(stderr, "Error initializing SDL audio: %s\n",
			SDL_GetError());
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
		fprintf(stderr, "Unknown audio format! (%u)\n",
			(int)specs->format);
		SDL_AudioQuit();
		return false;
	}
	fmt.channels = specs->channels;
	fmt.samples = specs->samples;
	fmt.callback = sdl_mix;

	/* Fill user data with provided info */
	audio_data.callback = specs->mix;
	audio_data.priv_data = specs->data;

	/* Open the audio device */
	if (SDL_OpenAudio(&fmt, NULL) < 0) {
		fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
		SDL_AudioQuit();
		return false;
	}

	return true;
}

void sdl_mix(void *UNUSED(unused), Uint8 *stream, int len)
{
	audio_data.callback(audio_data.priv_data, stream, len);
}

void sdl_start()
{
	SDL_PauseAudio(0);
}

void sdl_stop()
{
	SDL_PauseAudio(1);
}

void sdl_deinit()
{
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

AUDIO_START(sdl)
	.init = sdl_init,
	.start = sdl_start,
	.stop = sdl_stop,
	.deinit = sdl_deinit
AUDIO_END

