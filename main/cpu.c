#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cpu.h>
#include <list.h>
#ifdef __APPLE__
#include <mach-o/getsect.h>
#endif

#ifdef __APPLE__
void cx_cpus() __attribute__((__constructor__));
#endif

#if defined(_WIN32)
extern struct cpu _cpus_begin, _cpus_end;
static struct cpu *cpus_begin = &_cpus_begin;
static struct cpu *cpus_end = &_cpus_end;
#elif defined(__APPLE__)
static struct cpu *cpus_begin;
static struct cpu *cpus_end;
#else
extern struct cpu __cpus_begin, __cpus_end;
static struct cpu *cpus_begin = &__cpus_begin;
static struct cpu *cpus_end = &__cpus_end;
#endif

static struct list_link *cpu_instances;

#ifdef __APPLE__
void cx_cpus()
{
#ifdef __LP64__
	const struct section_64 *sect;
#else
	const struct section *sect;
#endif
	sect = getsectbyname(CPU_SEGMENT_NAME, CPU_SECTION_NAME);
	cpus_begin = (struct cpu *)(sect->addr);
	cpus_end = (struct cpu *)(sect->addr + sect->size);
}
#endif

bool cpu_add(struct cpu_instance *instance)
{
	struct cpu *cpu;
	for (cpu = cpus_begin; cpu < cpus_end; cpu++)
		if (!strcmp(instance->cpu_name, cpu->name)) {
			instance->cpu = cpu;
			if ((cpu->init && cpu->init(instance)) || !cpu->init) {
				list_insert(&cpu_instances, instance);
				return true;
			}
			return false;
		}

	/* Warn as CPU was not found */
	fprintf(stderr, "CPU \"%s\" not recognized!\n", instance->cpu_name);
	return false;
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

void cpu_remove_all()
{
	struct list_link *link = cpu_instances;
	struct cpu_instance *instance;

	while ((instance = list_get_next(&link)))
		if (instance->cpu->deinit)
			instance->cpu->deinit(instance);

	list_remove_all(&cpu_instances);
}

