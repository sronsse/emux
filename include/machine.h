#ifndef _MACHINE_H
#define _MACHINE_H

#include <stdbool.h>
#include <clock.h>
#include <cpu.h>
#include <controller.h>
#include <memory.h>

#define MACHINE_START(_name, _description) \
	static struct machine machine_##_name \
		__attribute__(( \
			__used__, \
			__section__("machines"), \
			__aligned__(__alignof__(struct machine)))) = { \
		.name = #_name, \
		.description = _description,
#define MACHINE_END \
	};

struct machine {
	char *name;
	char *description;
	struct clock_link *clocks;
	struct cpu_instance_link *cpu_instances;
	struct controller_instance_link *controller_instances;
	struct region_link *regions;
	uint64_t clock_rate;
	bool (*init)();
	void (*deinit)();
};

bool machine_init(char *name);
void machine_run();
void machine_deinit();

#endif

