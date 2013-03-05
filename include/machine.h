#ifndef _MACHINE_H
#define _MACHINE_H

#include <stdbool.h>
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
	bool (*init)();
	void (*deinit)();
	struct cpu_link *cpus;
	struct controller_link *controllers;
	struct region_link *regions;
};

bool machine_init(char *name);
void machine_deinit();

#endif

