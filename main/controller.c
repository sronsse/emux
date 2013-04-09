#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <controller.h>
#include <machine.h>

extern struct controller __controllers_begin, __controllers_end;
extern struct machine *machine;

void controller_add(struct controller_instance *instance)
{
	struct controller *c;
	for (c = &__controllers_begin; c < &__controllers_end; c++)
		if (!strcmp(instance->controller_name, c->name)) {
			instance->controller = c;
			if ((c->init && c->init(instance)) || !c->init)
				list_insert(&machine->controller_instances,
					instance);
			return;
		}

	/* Warn as controller was not found */
	fprintf(stderr, "Controller \"%s\" not recognized!\n",
		instance->controller_name);
}

void controller_remove_all()
{
	struct list_link *link = machine->controller_instances;
	struct controller_instance *instance;

	while ((instance = list_get_next(&link)))
		if (instance->controller->deinit)
			instance->controller->deinit(instance);

	list_remove_all(&machine->controller_instances);
}

