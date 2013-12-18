#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <controller.h>
#include <list.h>

struct list_link *controllers;
static struct list_link *controller_instances;

bool controller_add(struct controller_instance *instance)
{
	struct list_link *link = controllers;
	struct controller *c;

	while ((c = list_get_next(&link)))
		if (!strcmp(instance->controller_name, c->name)) {
			instance->controller = c;
			if ((c->init && c->init(instance)) || !c->init) {
				list_insert(&controller_instances,
					instance);
				return true;
			}
			return false;
		}

	/* Warn as controller was not found */
	fprintf(stderr, "Controller \"%s\" not recognized!\n",
		instance->controller_name);
	return false;
}

void controller_remove_all()
{
	struct list_link *link = controller_instances;
	struct controller_instance *instance;

	while ((instance = list_get_next(&link)))
		if (instance->controller->deinit)
			instance->controller->deinit(instance);

	list_remove_all(&controller_instances);
}

