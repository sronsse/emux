#ifndef _MACHINE_H
#define _MACHINE_H

#include <stdbool.h>
#include <list.h>

#define MACHINE_START(_name, _description) \
	static struct machine _machine = { \
		.name = #_name, \
		.description = _description,
#define MACHINE_END \
	}; \
	__attribute__((constructor)) static void _register() \
	{ \
		list_insert(&machines, &_machine); \
	} \
	__attribute__((destructor)) static void _unregister() \
	{ \
		list_remove(&machines, &_machine); \
	}

typedef void machine_priv_data_t;

struct machine {
	char *name;
	char *description;
	machine_priv_data_t *priv_data;
	bool running;
	bool (*init)(struct machine *machine);
	void (*deinit)(struct machine *machine);
};

bool machine_init();
void machine_run();
void machine_deinit();

extern struct list_link *machines;

#endif

