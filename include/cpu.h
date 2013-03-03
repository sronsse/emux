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

struct cpu {
	char *name;
	bool (*init)();
	void (*deinit)();
};

struct cpu_link {
	struct cpu *cpu;
	struct cpu_link *next;
};

void cpu_add(char *name);
void cpu_remove_all();

#endif

