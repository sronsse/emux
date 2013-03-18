#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <machine.h>

static void controller_insert(struct controller_instance *instance);

extern struct controller __controllers_begin, __controllers_end;
extern struct machine *machine;

void controller_insert(struct controller_instance *instance)
{
	struct controller_instance_link *link;
	struct controller_instance_link *tail;

	/* Create new link */
	link = malloc(sizeof(struct controller_instance_link));
	link->instance = instance;
	link->next = NULL;

	/* Set head if needed */
	if (!machine->controller_instances) {
		machine->controller_instances = link;
		return;
	}

	/* Find tail and add link */
	tail = machine->controller_instances;
	while (tail->next)
		tail = tail->next;
	tail->next = link;
}

void controller_add(struct controller_instance *instance)
{
	struct controller *c;
	for (c = &__controllers_begin; c < &__controllers_end; c++)
		if (!strcmp(instance->controller_name, c->name)) {
			instance->controller = c;
			if ((c->init && c->init(instance)) || !c->init)
				controller_insert(instance);
			return;
		}

	/* Warn as controller was not found */
	fprintf(stderr, "Controller \"%s\" not recognized!\n",
		instance->controller_name);
}

void controller_remove_all()
{
	struct controller_instance_link *link;
	while (machine->controller_instances) {
		link = machine->controller_instances;
		if (link->instance->controller->deinit)
			link->instance->controller->deinit(link->instance);
		machine->controller_instances =
			machine->controller_instances->next;
		free(link);
	}
}

