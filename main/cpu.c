#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cpu.h>
#include <machine.h>

extern struct cpu __cpus_begin, __cpus_end;
extern struct machine *machine;

void cpu_add(struct cpu_instance *instance)
{
	struct cpu *cpu;
	for (cpu = &__cpus_begin; cpu < &__cpus_end; cpu++)
		if (!strcmp(instance->cpu_name, cpu->name)) {
			instance->cpu = cpu;
			if ((cpu->init && cpu->init(instance)) || !cpu->init)
				list_insert(&machine->cpu_instances, instance);
			return;
		}

	/* Warn as CPU was not found */
	fprintf(stderr, "CPU \"%s\" not recognized!\n", instance->cpu_name);
}

void cpu_remove_all()
{
	struct list_link *link = machine->cpu_instances;
	struct cpu_instance *instance;

	while ((instance = list_get_next(&link)))
		if (instance->cpu->deinit)
			instance->cpu->deinit(instance);

	list_remove_all(&machine->cpu_instances);
}

