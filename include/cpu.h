#ifndef _CPU_H
#define _CPU_H

#include <stdbool.h>

#define CPU_SECTION_NAME	"cpus"
#ifdef __APPLE__
#define CPU_SEGMENT_NAME	"__DATA"
#define CPU_SECTION_LABEL	CPU_SEGMENT_NAME ", " CPU_SECTION_NAME
#else
#define CPU_SECTION_LABEL	CPU_SECTION_NAME
#endif

#define CPU_START(_name) \
	static struct cpu cpu_##_name \
		__attribute__(( \
			__used__, \
			__section__(CPU_SECTION_LABEL), \
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
	struct resource *resources;
	int num_resources;
	cpu_mach_data_t *mach_data;
	cpu_priv_data_t *priv_data;
	struct cpu *cpu;
};

void cpu_add(struct cpu_instance *instance);
void cpu_remove_all();

#endif

