#ifndef _CPU_H
#define _CPU_H

#include <stdbool.h>

#define CPU_START(_name) \
	static struct cpu cpu_##_name \
		__attribute__(( \
			__used__, \
			__section__("cpus"), \
			__aligned__(__alignof__(struct cpu)))) = { \
		.name = #_name,
#define CPU_END \
	};

typedef void cpu_mach_data_t;
typedef void cpu_priv_data_t;

struct cpu_instance;

struct cpu {
	char *name;
	bool (*init)(struct cpu_instance *instance);
	void (*deinit)(struct cpu_instance *instance);
};

struct cpu_instance {
	char *cpu_name;
	cpu_mach_data_t *mach_data;
	cpu_priv_data_t *priv_data;
	struct cpu *cpu;
};

struct cpu_instance_link {
	struct cpu_instance *instance;
	struct cpu_instance_link *next;
};

void cpu_add(struct cpu_instance *instance);
void cpu_remove_all();

#endif

