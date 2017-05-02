#ifndef _MACHINE_H
#define _MACHINE_H

#include <stdbool.h>
#include <list.h>
#include <util.h>

#define MACHINE_START(_name, _description) \
	static struct machine _machine = { \
		.name = #_name, \
		.description = _description,
#define MACHINE_END \
	}; \
	static void _unregister(void) \
	{ \
		list_remove(&machines, &_machine); \
	} \
	INITIALIZER(_register) \
	{ \
		list_insert(&machines, &_machine); \
		atexit(_unregister); \
	}

typedef void machine_priv_data_t;

struct machine {
	char *name;
	char *description;
	machine_priv_data_t *priv_data;
	bool running;
	bool (*init)(struct machine *machine);
	void (*reset)(struct machine *machine);
	void (*deinit)(struct machine *machine);
};

bool machine_init();
void machine_reset();
void machine_run();
void machine_step();
void machine_deinit();

extern struct list_link *machines;

#endif

