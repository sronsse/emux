#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include <stdbool.h>
#include <resource.h>

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

struct controller_instance;

struct controller {
	char *name;
	bool (*init)(struct controller_instance *instance);
	void (*deinit)(struct controller_instance *instance);
};

struct controller_instance {
	char *controller_name;
	struct resource *resources;
	int num_resources;
	controller_mach_data_t *mach_data;
	controller_priv_data_t *priv_data;
	struct controller *controller;
};

struct controller_instance_link {
	struct controller_instance *instance;
	struct controller_instance_link *next;
};

void controller_add(struct controller_instance *instance);
void controller_remove_all();

#endif

