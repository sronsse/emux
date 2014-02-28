#include <libretro.h>
#include <input.h>
#include <util.h>

struct retro_data {
	retro_input_poll_t input_poll_cb;
	retro_input_state_t input_state_cb;
};

static void ret_update(struct input_frontend *fe);

static struct retro_data retro_data;

void retro_set_input_poll(retro_input_poll_t cb)
{
	retro_data.input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
	retro_data.input_state_cb = cb;
}

void ret_update(struct input_frontend *UNUSED(fe))
{
	/* Poll input */
	retro_data.input_poll_cb();
}

INPUT_START(retro)
	.update = ret_update
INPUT_END

