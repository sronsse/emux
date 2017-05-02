#ifndef _CMDLINE_H
#define _CMDLINE_H

#include <stdbool.h>
#include <stdlib.h>
#include <util.h>

#define PARAM(_var, _type, _name, _module, _desc) \
	static struct param _param_##_var = { \
		.address = &_var, \
		.type = #_type, \
		.name = _name, \
		.module = _module, \
		.desc = _desc \
	}; \
	static void _unregister_param_##_var(void) \
	{ \
		cmdline_unregister_param(&_param_##_var); \
	} \
	INITIALIZER(_register_param_##_var) \
	{ \
		cmdline_register_param(&_param_##_var); \
		atexit(_unregister_param_##_var); \
	}

struct param {
	void *address;
	char *type;
	char *name;
	char *module;
	char *desc;
};

void cmdline_register_param(struct param *param);
void cmdline_unregister_param(struct param *param);
bool cmdline_set_param(char *name, char *module, char *value);
void cmdline_init(int argc, char *argv[]);
void cmdline_print_usage(bool error);

#endif

