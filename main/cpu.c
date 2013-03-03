#include <stdio.h>
#include <stdlib.h>
#include <machine.h>

static void cpu_insert(struct cpu *cpu);

extern struct cpu __cpus_begin, __cpus_end;
extern struct machine *machine;

void cpu_insert(struct cpu *cpu)
{
	struct cpu_link *link;
	struct cpu_link *tail;

	/* Create new link */
	link = malloc(sizeof(struct cpu_link));
	link->cpu = cpu;
	link->next = NULL;

	/* Set head if needed */
	if (!machine->cpus) {
		machine->cpus = link;
		return;
	}

	/* Find tail and add link */
	tail = machine->cpus;
	while (tail->next)
		tail = tail->next;
	tail->next = link;
}

void cpu_add(char *name)
{
	struct cpu *cpu;
	for (cpu = &__cpus_begin; cpu < &__cpus_end; cpu++)
		if (!strcmp(name, cpu->name)) {
			if (cpu->init())
				cpu_insert(cpu);
			return;
		}

	/* Warn as CPU was not found */
	fprintf(stderr, "CPU \"%s\" not recognized!\n", name);
}

void cpu_remove_all()
{
	struct cpu_link *link;
	while (machine->cpus) {
		link = machine->cpus;
		link->cpu->deinit();
		machine->cpus = machine->cpus->next;
		free(link);
	}
}

