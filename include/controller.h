#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include <stdbool.h>
#include <list.h>
#include <resource.h>
#include <util.h>

#define CONTROLLER_START(_name) \
	static struct controller _controller = { \
		.name = #_name,
#define CONTROLLER_END \
	}; \
	static void _unregister(void) \
	{ \
		list_remove(&controllers, &_controller); \
	} \
	INITIALIZER(_register) \
	{ \
		list_insert(&controllers, &_controller); \
		atexit(_unregister); \
	}

typedef void controller_mach_data_t;
typedef void controller_priv_data_t;

struct controller_instance;

struct controller {
	char *name;
	bool (*init)(struct controller_instance *instance);
	void (*reset)(struct controller_instance *instance);
	void (*deinit)(struct controller_instance *instance);
};

struct controller_instance {
	char *controller_name;
	int bus_id;
	struct resource *resources;
	int num_resources;
	controller_mach_data_t *mach_data;
	controller_priv_data_t *priv_data;
	struct controller *controller;
};

bool controller_add(struct controller_instance *instance);
void controller_reset_all();
void controller_remove_all();

extern struct list_link *controllers;

#endif

