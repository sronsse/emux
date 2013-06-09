#include <stdio.h>
#include <string.h>
#include <cmdline.h>
#include <audio.h>
#ifdef __APPLE__
#include <mach-o/getsect.h>
#endif

#ifdef __APPLE__
void cx_audio_frontends() __attribute__((__constructor__));
#endif

#if defined(_WIN32)
extern struct audio_frontend _audio_frontends_begin, _audio_frontends_end;
static struct audio_frontend *audio_frontends_begin = &_audio_frontends_begin;
static struct audio_frontend *audio_frontends_end = &_audio_frontends_end;
#elif defined(__APPLE__)
struct audio_frontend *audio_frontends_begin;
struct audio_frontend *audio_frontends_end;
#else
extern struct audio_frontend __audio_frontends_begin, __audio_frontends_end;
static struct audio_frontend *audio_frontends_begin = &__audio_frontends_begin;
static struct audio_frontend *audio_frontends_end = &__audio_frontends_end;
#endif

static struct audio_frontend *frontend;

#ifdef __APPLE__
void cx_audio_frontends()
{
#ifdef __LP64__
	const struct section_64 *sect;
#else
	const struct section *sect;
#endif
	sect = getsectbyname(AUDIO_SEGMENT_NAME, AUDIO_SECTION_NAME);
	audio_frontends_begin = (struct audio_frontend *)(sect->addr);
	audio_frontends_end = (struct audio_frontend *)(sect->addr +
		sect->size);
}
#endif

bool audio_init(struct audio_specs *specs)
{
	struct audio_frontend *fe;
	char *name;

	if (frontend) {
		fprintf(stderr, "Audio frontend already initialized!\n");
		return false;
	}

	/* Get selected frontend name */
	if (!cmdline_parse_string("audio", &name)) {
		fprintf(stderr, "No audio frontend selected!\n");
		return false;
	}

	/* Find frontend and initialize it */
	for (fe = audio_frontends_begin; fe < audio_frontends_end; fe++)
		if (!strcmp(name, fe->name)) {
			if ((fe->init && fe->init(specs))) {
				frontend = fe;
				return true;
			}
			return false;
		}

	/* Warn as audio frontend was not found */
	fprintf(stderr, "Audio frontend \"%s\" not recognized!\n", name);
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

