#ifndef _MACHINE_H
#define _MACHINE_H

#include <stdbool.h>
#include <stdint.h>
#include <list.h>

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
	struct list_link *clocks;
	struct list_link *cpu_instances;
	struct list_link *controller_instances;
	struct list_link *regions;
	uint64_t clock_rate;
	bool (*init)();
	void (*deinit)();
};

bool machine_init(char *name);
void machine_run();
void machine_deinit();

#endif

