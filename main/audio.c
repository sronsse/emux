#include <stdio.h>
#include <string.h>
#include <audio.h>
#include <cmdline.h>
#include <list.h>
#include <log.h>

#define DEFAULT_SAMPLING_RATE 44100

/* Command-line parameter */
static char *audio_fe_name;
PARAM(audio_fe_name, string, "audio", NULL, "Selects audio frontend")
static int sampling_rate = DEFAULT_SAMPLING_RATE;
PARAM(sampling_rate, int, "sampling-rate", NULL, "Sets audio sampling rate")

struct list_link *audio_frontends;
static struct audio_frontend *frontend;

bool audio_init(struct audio_specs *specs)
{
	struct list_link *link = audio_frontends;
	struct audio_frontend *fe;

	if (frontend) {
		LOG_E("Audio frontend already initialized!\n");
		return false;
	}

	/* Validate audio option */
	if (!audio_fe_name) {
		LOG_W("No audio frontend selected!\n");
		return true;
	}

	/* Validate audio sampling rate */
	switch (sampling_rate) {
	case 11025:
	case 22050:
	case 44100:
	case 48000:
		break;
	default:
		LOG_W("%u Hz sampling rate not supported.\n", sampling_rate);
		LOG_W("Please select 11025, 22050, 44100, or 48000 Hz.\n");
		sampling_rate = DEFAULT_SAMPLING_RATE;
		break;
	}

	/* Find audio frontend */
	while ((fe = list_get_next(&link))) {
		/* Skip if name does not match */
		if (strcmp(audio_fe_name, fe->name))
			continue;

		/* Initialize frontend */
		specs->sampling_rate = sampling_rate;
		if (fe->init && !fe->init(fe, specs))
			return false;

		/* Save frontend and return success */
		frontend = fe;
		return true;
	}

	/* Warn as audio frontend was not found */
	LOG_E("Audio frontend \"%s\" not recognized!\n", audio_fe_name);
	return false;
}

void audio_enqueue(void *buffer, int length)
{
	if (frontend && frontend->enqueue)
		frontend->enqueue(frontend, buffer, length);
}

void audio_start()
{
	if (frontend && frontend->start)
		frontend->start(frontend);
}

void audio_stop()
{
	if (frontend && frontend->stop)
		frontend->stop(frontend);
}

void audio_deinit()
{
	if (!frontend)
		return;

	if (frontend->deinit)
		frontend->deinit(frontend);
	frontend = NULL;
}

