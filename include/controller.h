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

typedef void controller_mach_data_t;
typedef void controller_priv_data_t;

struct controller {
	char *name;
	bool (*init)(struct controller *controller);
	void (*deinit)(struct controller *controller);
	controller_mach_data_t *mach_data;
	controller_priv_data_t *priv_data;
};

struct controller_link {
	struct controller *controller;
	struct controller_link *next;
};

void controller_add(char *name, controller_mach_data_t *mach_data);
void controller_remove_all();

#endif

