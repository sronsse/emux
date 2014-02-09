#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <config.h>
#include <cmdline.h>
#include <env.h>
#include <input.h>
#include <list.h>
#include <log.h>
#ifdef LIBRETRO
#undef CONFIG_INPUT_XML
#endif
#ifdef CONFIG_INPUT_XML
#include <roxml.h>
#endif

#ifdef CONFIG_INPUT_XML
/* Configuration file and node definitions */
#define MAX_DOC_PATH_LENGTH	1024
#define DOC_FILENAME		"config.xml"
#define DOC_CONFIG_NODE_NAME	"config"
#define DOC_KEY_NODE_NAME	"key"
#endif

struct list_link *input_frontends;
static struct input_frontend *frontend;
static struct list_link *listeners;
#ifdef CONFIG_INPUT_XML
static node_t *config_doc;
#endif

bool input_init(char *name)
{
	struct list_link *link = input_frontends;
	struct input_frontend *fe;
	video_window_t *window;
#ifdef CONFIG_INPUT_XML
	char doc_path[MAX_DOC_PATH_LENGTH + 1];
#endif

	if (frontend) {
		LOG_E("Input frontend already initialized!\n");
		return false;
	}

#ifdef CONFIG_INPUT_XML
	LOG_D("Opening input configuration file.\n");

	/* Set config doc path */
	snprintf(doc_path,
		MAX_DOC_PATH_LENGTH,
		"%s/%s",
		env_get_config_path(),
		DOC_FILENAME);

	/* Load input config file and fall back to original path if needed */
	config_doc = roxml_load_doc(doc_path);
	if (!config_doc)
		config_doc = roxml_load_doc(DOC_FILENAME);

	/* Warn if file could not be opened */
	if (!config_doc)
		LOG_W("Could not open input configuration file!\n");
#endif

	/* Get window from video frontend */
	window = video_get_window();

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
	LOG_E("Input frontend \"%s\" not recognized!\n", name);
	return false;
}

bool input_load(char *name, struct input_event *events, int num_events)
{
	/* Leave already if input frontend is not initialized */
	if (!frontend)
		return false;

#ifdef CONFIG_INPUT_XML
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
		return false;

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
		LOG_W("Error parsing input configuration file!\n");
	return rc;
#else
	/* Do nothing */
	(void)name;
	(void)events;
	(void)num_events;
	return false;
#endif
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
#ifdef CONFIG_INPUT_XML
	if (config_doc)
		roxml_close(config_doc);
#endif

	if (!frontend)
		return;

	if (frontend->deinit)
		frontend->deinit();
	frontend = NULL;
}

