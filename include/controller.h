#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include <stdbool.h>

#define CONTROLLER_START(_name) \
	static struct controller controller_##_name \
		__attribute__(( \
			__used__, \
			__section__("controllers"), \
			__aligned__(__alignof__(struct controller)))) = { \
		.name = #_name,
#define CONTROLLER_END \
	};

typedef void machine_data_t;

struct controller {
	char *name;
	void (*init)(machine_data_t *data);
	void (*deinit)();
};

struct controller_link {
	struct controller *controller;
	struct controller_link *next;
};

void controller_add(char *name, machine_data_t *data);
void controller_remove_all();

#endif

