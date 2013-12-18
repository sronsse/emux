#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <roxml.h>
#include <cmdline.h>
#include <input.h>
#include <list.h>

/* Configuration file and node definitions */
#define DOC_FILENAME		"config.xml"
#define DOC_CONFIG_NODE_NAME	"config"
#define DOC_KEY_NODE_NAME	"key"

struct list_link *input_frontends;
static struct input_frontend *frontend;
static struct list_link *listeners;
static node_t *config_doc;

bool input_init(char *name, video_window_t *window)
{
	struct list_link *link = input_frontends;
	struct input_frontend *fe;

	if (frontend) {
		fprintf(stderr, "Input frontend already initialized!\n");
		return false;
	}

	/* Load input configuration file */
	config_doc = roxml_load_doc(DOC_FILENAME);

	/* Find input frontend and initialize it */
	while ((fe = list_get_next(&link)))
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

bool input_load(char *name, struct input_event *events, int num_events)
{
	node_t *node;
	node_t *child;
	int i;
	char *str;
	char *end;
	int key;
	int size;
	bool rc = false;

	/* Check if configuration file was loaded */
	if (!config_doc)
		goto err;

	/* Find document initial node */
	node = roxml_get_chld(config_doc, DOC_CONFIG_NODE_NAME, 0);
	if (!node)
		goto err;

	/* Find appropriate section */
	node = roxml_get_chld(node, name, 0);
	if (!node)
		goto err;

	/* Get number of entries and check for validity */
	if (roxml_get_chld_nb(node) != num_events)
		goto err;

	/* Parse children and create matching events */
	for (i = 0; i < num_events; i++) {
		child = roxml_get_chld(node, NULL, i);
		if (!child)
			goto err;

		/* Check for event type */
		str = roxml_get_name(child, NULL, 0);
		if (!strcmp(str, DOC_KEY_NODE_NAME)) {
			str = roxml_get_content(child, NULL, 0, &size);

			/* Get key value */
			key = strtol(str, &end, 10);
			if (*end)
				goto err;

			/* Set event */
			events[i].type = EVENT_KEYBOARD;
			events[i].keyboard.key = key;
		} else {
			/* We should never reach here */
			goto err;
		}
	}

	/* Configuration was loaded properly */
	rc = true;
err:
	roxml_release(RELEASE_ALL);
	if (!rc)
		fprintf(stderr, "Error parsing input configuration file!\n");
	return rc;
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
	roxml_close(config_doc);
	frontend = NULL;
}

