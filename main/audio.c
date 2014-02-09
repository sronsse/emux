#include <stdio.h>
#include <string.h>
#include <audio.h>
#include <cmdline.h>
#include <list.h>
#include <log.h>

/* Command-line parameter */
static char *audio_fe_name;
PARAM(audio_fe_name, string, "audio", NULL, "Selects audio frontend")

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

	/* Find frontend and initialize it */
	while ((fe = list_get_next(&link)))
		if (!strcmp(audio_fe_name, fe->name)) {
			if ((fe->init && fe->init(specs))) {
				frontend = fe;
				return true;
			}
			return false;
		}

	/* Warn as audio frontend was not found */
	LOG_E("Audio frontend \"%s\" not recognized!\n", audio_fe_name);
	return false;
}

void audio_start()
{
	if (frontend && frontend->start)
		frontend->start();
}

void audio_stop()
{
	if (frontend && frontend->stop)
		frontend->stop();
}

void audio_deinit()
{
	if (!frontend)
		return;

	if (frontend->deinit)
		frontend->deinit();
	frontend = NULL;
}

