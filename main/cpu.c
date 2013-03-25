#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <machine.h>

static void cpu_insert(struct cpu_instance *instance);

extern struct cpu __cpus_begin, __cpus_end;
extern struct machine *machine;

void cpu_insert(struct cpu_instance *instance)
{
	struct cpu_instance_link *link;
	struct cpu_instance_link *tail;

	/* Create new link */
	link = malloc(sizeof(struct cpu_instance_link));
	link->instance = instance;
	link->next = NULL;

	/* Set head if needed */
	if (!machine->cpu_instances) {
		machine->cpu_instances = link;
		return;
	}

	/* Find tail and add link */
	tail = machine->cpu_instances;
	while (tail->next)
		tail = tail->next;
	tail->next = link;
}

void cpu_add(struct cpu_instance *instance)
{
	struct cpu *cpu;
	for (cpu = &__cpus_begin; cpu < &__cpus_end; cpu++)
		if (!strcmp(instance->cpu_name, cpu->name)) {
			instance->cpu = cpu;
			if ((cpu->init && cpu->init(instance)) || !cpu->init)
				cpu_insert(instance);
			return;
		}

	/* Warn as CPU was not found */
	fprintf(stderr, "CPU \"%s\" not recognized!\n", instance->cpu_name);
}

void cpu_remove_all()
{
	struct cpu_instance_link *link;
	while (machine->cpu_instances) {
		link = machine->cpu_instances;
		if (link->instance->cpu->deinit)
			link->instance->cpu->deinit(link->instance);
		machine->cpu_instances = machine->cpu_instances->next;
		free(link);
	}
}

