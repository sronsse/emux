#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <controller.h>
#include <machine.h>
#ifdef __APPLE__
#include <mach-o/getsect.h>
#endif

#ifdef __APPLE__
void cx_controllers() __attribute__((__constructor__));
#endif

#if defined(_WIN32)
extern struct controller _controllers_begin, _controllers_end;
static struct controller *controllers_begin = &_controllers_begin;
static struct controller *controllers_end = &_controllers_end;
#elif defined(__APPLE__)
static struct controller *controllers_begin;
static struct controller *controllers_end;
#else
extern struct controller __controllers_begin, __controllers_end;
static struct controller *controllers_begin = &__controllers_begin;
static struct controller *controllers_end = &__controllers_end;
#endif

extern struct machine *machine;

#ifdef __APPLE__
void cx_controllers()
{
#ifdef __LP64__
	const struct section_64 *sect;
#else
	const struct section *sect;
#endif
	sect = getsectbyname(CONTROLLER_SEGMENT_NAME, CONTROLLER_SECTION_NAME);
	controllers_begin = (struct controller *)(sect->addr);
	controllers_end = (struct controller *)(sect->addr + sect->size);
}
#endif

void controller_add(struct controller_instance *instance)
{
	struct controller *c;
	for (c = controllers_begin; c < controllers_end; c++)
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

