#include <stdbool.h>
#include <audio.h>
#include <libretro.h>
#include <util.h>

struct retro_data {
	int sampling_rate;
	retro_audio_sample_t audio_cb;
	bool enabled;
};

void retro_audio_fill_timing(struct retro_system_timing *timing);

static bool ret_init(struct audio_frontend *fe, int sampling_rate);
static void ret_enqueue(struct audio_frontend *fe, int16_t left, int16_t right);
static void ret_start(struct audio_frontend *fe);
static void ret_stop(struct audio_frontend *fe);

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
	timing->sample_rate = retro_data.sampling_rate;
}

bool ret_init(struct audio_frontend *UNUSED(fe), int sampling_rate)
{
	/* Set audio properties */
	retro_data.sampling_rate = sampling_rate;
	retro_data.enabled = false;
	return true;
}

void ret_enqueue(struct audio_frontend *UNUSED(fe), int16_t left, int16_t right)
{
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

