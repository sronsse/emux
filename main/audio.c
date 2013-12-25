#include <stdio.h>
#include <string.h>
#include <audio.h>
#include <cmdline.h>
#include <list.h>
#include <log.h>

struct list_link *audio_frontends;
static struct audio_frontend *frontend;

bool audio_init(struct audio_specs *specs)
{
	struct list_link *link = audio_frontends;
	struct audio_frontend *fe;
	char *name;

	if (frontend) {
		LOG_E("Audio frontend already initialized!\n");
		return false;
	}

	/* Get selected frontend name */
	if (!cmdline_parse_string("audio", &name)) {
		LOG_E("No audio frontend selected!\n");
		return false;
	}

	/* Find frontend and initialize it */
	while ((fe = list_get_next(&link)))
		if (!strcmp(name, fe->name)) {
			if ((fe->init && fe->init(specs))) {
				frontend = fe;
				return true;
			}
			return false;
		}

	/* Warn as audio frontend was not found */
	LOG_E("Audio frontend \"%s\" not recognized!\n", name);
	return false;
}

void audio_start()
{
	if (frontend->start)
		frontend->start();
}

void audio_stop()
{
	if (frontend->stop)
		frontend->stop();
}

void audio_deinit()
{
	if (frontend->deinit)
		frontend->deinit();
	frontend = NULL;
}

