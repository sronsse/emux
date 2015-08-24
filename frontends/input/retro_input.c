#include <stdlib.h>
#include <string.h>
#include <libretro.h>
#include <input.h>
#include <util.h>

struct input_data {
	struct input_config *config;
	int16_t *states;
};

struct retro_data {
	struct list_link *input_states;
	retro_input_poll_t input_poll_cb;
	retro_input_state_t input_state_cb;
};

static void ret_load(struct input_frontend *fe, struct input_config *cfg);
static void ret_unload(struct input_frontend *fe, struct input_config *cfg);
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

void ret_load(struct input_frontend *fe, struct input_config *cfg)
{
	struct input_data *data;

	/* Allocate input data structure and append it to states list */
	data = calloc(1, sizeof(struct input_data));
	list_insert(&retro_data.input_states, data);

	/* Initialize input data */
	data->config = cfg;
	data->states = calloc(cfg->num_descs, sizeof(int16_t));
}

void ret_unload(struct input_frontend *fe, struct input_config *cfg)
{
	struct list_link *link = retro_data.input_states;
	struct input_data *data;

	/* Find data associated to config */
	while ((data = list_get_next(&link)))
		if (data->config == cfg)
			break;

	/* Free data and remove it from states list */
	free(data->states);
	free(data);
	list_remove(&retro_data.input_states, data);
}

void ret_update(struct input_frontend *UNUSED(fe))
{
	struct list_link *link = retro_data.input_states;
	struct input_data *data;
	struct input_desc *desc;
	enum input_type type;
	unsigned int dev;
	unsigned int port;
	unsigned int index;
	unsigned int id;
	int16_t s;
	int i;

	/* Poll input */
	retro_data.input_poll_cb();

	/* Parse all configs and update states */
	while ((data = list_get_next(&link)))
		for (i = 0; i < data->config->num_descs; i++) {
			/* Get descriptor */
			desc = &data->config->descs[i];

			/* Extract device, port, index, and ID from config */
			dev = desc->device;
			port = (desc->code >> PORT_SHIFT) & PORT_MASK;
			index = (desc->code >> INDEX_SHIFT) & INDEX_MASK;
			id = (desc->code >> ID_SHIFT) & ID_MASK;

			/* Get state and continue if unchanged */
			s = retro_data.input_state_cb(port, dev, index, id);
			if (s == data->states[i])
				continue;

			/* Report event directly */
			type = (s > 0) ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;
			data->config->callback(i, type, data->config->data);

			/* Save new state */
			data->states[i] = s;
		}
}

INPUT_START(retro)
	.load = ret_load,
	.unload = ret_unload,
	.update = ret_update
INPUT_END

