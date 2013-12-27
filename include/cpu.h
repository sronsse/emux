#ifndef _CPU_H
#define _CPU_H

#include <stdbool.h>
#include <list.h>

#define CPU_START(_name) \
	static struct cpu _cpu = { \
		.name = #_name,
#define CPU_END \
	}; \
	__attribute__((constructor)) static void _register() \
	{ \
		list_insert(&cpus, &_cpu); \
	} \
	__attribute__((destructor)) static void _unregister() \
	{ \
		list_remove(&cpus, &_cpu); \
	}

typedef void cpu_mach_data_t;
typedef void cpu_priv_data_t;

struct cpu_instance;

struct cpu {
	char *name;
	bool (*init)(struct cpu_instance *instance);
	void (*reset)(struct cpu_instance *instance);
	void (*interrupt)(struct cpu_instance *instance, int irq);
	void (*deinit)(struct cpu_instance *instance);
};

struct cpu_instance {
	char *cpu_name;
	int bus_id;
	struct resource *resources;
	int num_resources;
	cpu_mach_data_t *mach_data;
	cpu_priv_data_t *priv_data;
	struct cpu *cpu;
};

bool cpu_add(struct cpu_instance *instance);
void cpu_reset_all();
void cpu_interrupt(int irq);
void cpu_remove_all();

extern struct list_link *cpus;

#endif

