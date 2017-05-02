#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <audio.h>
#include <cmdline.h>
#include <list.h>
#include <log.h>

#define DEFAULT_SAMPLING_RATE 48000

struct resample_data {
	enum audio_format format;
	int num_channels;
	float mul;
	float step;
	int count;
	int left;
	int right;
};

static int16_t audio_get_sample(void **buffer);

/* Command-line parameter */
static char *audio_fe_name;
PARAM(audio_fe_name, string, "audio", NULL, "Selects audio frontend")
static int sampling_rate = DEFAULT_SAMPLING_RATE;
PARAM(sampling_rate, int, "sampling-rate", NULL, "Sets audio sampling rate")

struct list_link *audio_frontends;
static struct audio_frontend *frontend;
static struct resample_data resample_data;

int16_t audio_get_sample(void **buffer)
{
	int16_t v = 0;

	/* Get value based on format */
	switch (resample_data.format) {
	case AUDIO_FORMAT_U8:
		v = *((uint8_t *)*buffer);
		v -= UCHAR_MAX / 2;
		v <<= 8;
		*(uint8_t **)buffer += sizeof(uint8_t);
		break;
	case AUDIO_FORMAT_S8:
		v = *((int8_t *)*buffer);
		v <<= 8;
		*(int8_t **)buffer += sizeof(int8_t);
		break;
	case AUDIO_FORMAT_U16:
		v = *((uint16_t *)*buffer);
		v -= USHRT_MAX / 2;
		*(uint16_t **)buffer += sizeof(uint16_t);
		break;
	case AUDIO_FORMAT_S16:
		v = *((int16_t *)*buffer);
		*(int16_t **)buffer += sizeof(int16_t);
		break;
	}

	/* Return value */
	return v;
}

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
		if (fe->init && !fe->init(fe, sampling_rate))
			return false;

		/* Save frontend */
		frontend = fe;

		/* Initialize resampling data */
		resample_data.format = specs->format;
		resample_data.num_channels = specs->channels;
		resample_data.mul = sampling_rate / specs->freq;
		resample_data.step = 0.0f;
		resample_data.count = 0;
		resample_data.left = 0;
		resample_data.right = 0;

		/* Return success */
		return true;
	}

	/* Warn as audio frontend was not found */
	LOG_E("Audio frontend \"%s\" not recognized!\n", audio_fe_name);
	return false;
}

void audio_enqueue(void *buffer, int length)
{
	bool stereo = (resample_data.num_channels == 2);
	bool reset;
	float prev_step;
	int16_t left;
	int16_t right;
	int i;

	/* Return if needed */
	if (!frontend || !frontend->enqueue)
		return;

	/* Parse all input buffer samples */
	for (i = 0; i < length; i++) {
		/* Get left (or mono) value */
		resample_data.left += audio_get_sample(&buffer);

		/* Get right value if needed */
		if (stereo)
			resample_data.right += audio_get_sample(&buffer);

		/* Increment resample data count */
		resample_data.count++;

		/* Compute next step until output is no longer generated */
		reset = false;
		prev_step = resample_data.step;
		resample_data.step = prev_step + resample_data.mul;
		while ((int)prev_step != (int)resample_data.step) {
			/* Compute final left/right (or mono) samples */
			left = resample_data.left / resample_data.count;
			right = !stereo ?
				left :
				resample_data.right / resample_data.count;

			/* Push left/right pair to frontend */
			frontend->enqueue(frontend, left, right);

			/* Update step and request state reset */
			resample_data.step -= 1.0f;
			reset = true;
		}

		/* Reset state if required */
		if (reset) {
			resample_data.count = 0;
			resample_data.left = 0;
			resample_data.right = 0;
		}
	}
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

