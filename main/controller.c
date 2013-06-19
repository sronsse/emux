#include <stdio.h>
#include <stdlib.h>
#include <machine.h>

static void controller_insert(struct controller *controller);

extern struct controller __controllers_begin, __controllers_end;
extern struct machine *machine;

void controller_insert(struct controller *controller)
{
	struct controller_link *link;
	struct controller_link *tail;

	/* Create new link */
	link = malloc(sizeof(struct controller_link));
	link->controller = controller;
	link->next = NULL;

	/* Set head if needed */
	if (!machine->controllers) {
		machine->controllers = link;
		return;
	}

	/* Find tail and add link */
	tail = machine->controllers;
	while (tail->next)
		tail = tail->next;
	tail->next = link;
}

void controller_add(char *name, machine_data_t *mdata)
{
	struct controller *c;
	for (c = &__controllers_begin; c < &__controllers_end; c++)
		if (!strcmp(name, c->name)) {
			controller_insert(c);
			c->mdata = mdata;
			c->init(c);
			return;
		}

	/* Warn as controller was not found */
	fprintf(stderr, "Controller \"%s\" not recognized!\n", name);
}

void controller_remove_all()
{
	struct controller_link *link;
	while (machine->controllers) {
		link = machine->controllers;
		link->controller->deinit(link->controller);
		machine->controllers = machine->controllers->next;
		free(link);
	}
}

