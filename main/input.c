#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmdline.h>
#include <input.h>
#include <list.h>

extern struct input_frontend __input_frontends_begin, __input_frontends_end;

static struct input_frontend *frontend;
static struct list_link *listeners;

bool input_init(char *name, video_window_t *window)
{
	struct input_frontend *fe;

	if (frontend) {
		fprintf(stderr, "Input frontend already initialized!\n");
		return false;
	}

	/* Find input frontend and initialize it */
	for (fe = &__input_frontends_begin; fe < &__input_frontends_end; fe++)
		if (!strcmp(name, fe->name)) {
			if ((fe->init && fe->init(window))) {
				frontend = fe;
				return true;
			}
			return false;
		}

	/* Warn as input frontend was not found */
	fprintf(stderr, "Input frontend \"%s\" not recognized!\n", name);
	return false;
}

void input_update()
{
	if (frontend && frontend->update)
		frontend->update();
}

void input_report(struct input_event *event, struct input_state *state)
{
	struct list_link *link = listeners;
	struct input_config *config;
	struct input_event *e;
	int i;

	while ((config = list_get_next(&link)))
		for (i = 0; i < config->num_events; i++) {
			e = &config->events[i];
			/* Skip event if type is not matched */
			if (event->type != config->events[i].type)
				continue;

			/* Skip event if other members are not matched */
			switch (event->type) {
			case EVENT_KEYBOARD:
				if (event->keyboard.key != e->keyboard.key)
					continue;
				break;
			default:
				break;
			}

			/* Call registered listener */
			config->callback(i, state, config->data);
		}
}

void input_register(struct input_config *config)
{
	list_insert(&listeners, config);
}

void input_unregister(struct input_config *config)
{
	list_remove(&listeners, config);
}

void input_deinit()
{
	if (frontend->deinit)
		frontend->deinit();
	frontend = NULL;
}

