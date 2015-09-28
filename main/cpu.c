#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cpu.h>
#include <list.h>
#include <log.h>

struct list_link *cpus;
static struct list_link *cpu_instances;

bool cpu_add(struct cpu_instance *instance)
{
	struct list_link *link = cpus;
	struct cpu *cpu;

	while ((cpu = list_get_next(&link)))
		if (!strcmp(instance->cpu_name, cpu->name)) {
			instance->cpu = cpu;
			if ((cpu->init && cpu->init(instance)) || !cpu->init) {
				list_insert(&cpu_instances, instance);
				return true;
			}
			return false;
		}

	/* Warn as CPU was not found */
	LOG_E("CPU \"%s\" not recognized!\n", instance->cpu_name);
	return false;
}

void cpu_reset_all()
{
	struct list_link *link = cpu_instances;
	struct cpu_instance *instance;

	while ((instance = list_get_next(&link)))
		if (instance->cpu->reset)
			instance->cpu->reset(instance);
}

void cpu_interrupt(int irq)
{
	struct cpu_instance *instance;
	struct list_link *link = cpu_instances;

	/* Interrupt first CPU only */
	instance = list_get_next(&link);
	if (instance->cpu && instance->cpu->interrupt)
		instance->cpu->interrupt(instance, irq);
}

void cpu_halt(bool halt)
{
	struct cpu_instance *instance;
	struct list_link *link = cpu_instances;

	/* Halt first CPU only */
	instance = list_get_next(&link);
	if (instance->cpu && instance->cpu->halt)
		instance->cpu->halt(instance, halt);
}

void cpu_remove_all()
{
	struct list_link *link = cpu_instances;
	struct cpu_instance *instance;

	while ((instance = list_get_next(&link)))
		if (instance->cpu->deinit)
			instance->cpu->deinit(instance);

	list_remove_all(&cpu_instances);
}

