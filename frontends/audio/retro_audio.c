#include <stdbool.h>
#include <audio.h>
#include <libretro.h>
#include <util.h>

struct retro_data {
	float freq;
	enum audio_format format;
	int channels;
	retro_audio_sample_t audio_cb;
	bool enabled;
};

void retro_audio_fill_timing(struct retro_system_timing *timing);

static bool ret_init(struct audio_frontend *fe, struct audio_specs *specs);
static void ret_enqueue(struct audio_frontend *fe, void *buffer, int count);
static void ret_start(struct audio_frontend *fe);
static void ret_stop(struct audio_frontend *fe);
static void get_value(void **buffer, int16_t *v);

static struct retro_data retro_data;

void retro_set_audio_sample(retro_audio_sample_t cb)
{
	/* Save audio sample callback */
	retro_data.audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t UNUSED(cb))
{
}

void retro_audio_fill_timing(struct retro_system_timing *timing)
{
	/* Fill sample rate */
	timing->sample_rate = retro_data.freq;
}

void get_value(void **buffer, int16_t *v)
{
	uint8_t **buf_u8 = (uint8_t **)buffer;
	int8_t **buf_s8 = (int8_t **)buffer;
	uint16_t **buf_u16 = (uint16_t **)buffer;
	int16_t **buf_s16 = (int16_t **)buffer;

	/* Get value based on format */
	switch (retro_data.format) {
	case AUDIO_FORMAT_U8:
		*v = (*(*buf_u8)++ << 8) - (USHRT_MAX / 2) - 1;
		break;
	case AUDIO_FORMAT_S8:
		*v = *(*buf_s8)++ << 8;
		break;
	case AUDIO_FORMAT_U16:
		*v = *(*buf_u16)++ - (USHRT_MAX / 2) - 1;
		break;
	case AUDIO_FORMAT_S16:
		*v = *(*buf_s16)++;
		break;
	}
}

bool ret_init(struct audio_frontend *UNUSED(fe), struct audio_specs *specs)
{
	/* Set audio properties */
	retro_data.freq = specs->freq;
	retro_data.format = specs->format;
	retro_data.channels = specs->channels;
	retro_data.enabled = false;
	return true;
}

void ret_enqueue(struct audio_frontend *UNUSED(fe), void *buffer, int count)
{
	int16_t left = 0;
	int16_t right = 0;

	/* Get buffer data only when audio is enabled */
	if (retro_data.enabled) {
		/* Get left channel */
		get_value(&buffer, &left);

		/* Get right channel */
		right = left;
		if (retro_data.channels == 2)
			get_value(&buffer, &right);
	}

	/* Push sample */
	retro_data.audio_cb(left, right);
}

void ret_start(struct audio_frontend *UNUSED(fe))
{
	/* Enable audio */
	retro_data.enabled = true;
}

void ret_stop(struct audio_frontend *UNUSED(fe))
{
	/* Disable audio */
	retro_data.enabled = false;
}

AUDIO_START(retro)
	.init = ret_init,
	.enqueue = ret_enqueue,
	.start = ret_start,
	.stop = ret_stop,
AUDIO_END

