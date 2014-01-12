#ifndef _CMDLINE_H
#define _CMDLINE_H

#include <stdbool.h>
#include <stdlib.h>

#define PARAM(_var, _type, _name, _module, _desc) \
	static struct param _param_##_var = { \
		.address = &_var, \
		.type = #_type, \
		.name = _name, \
		.module = _module, \
		.desc = _desc \
	}; \
	__attribute__((constructor)) static void _register_param_##_var() \
	{ \
		cmdline_register_param(&_param_##_var); \
	} \
	__attribute__((destructor)) static void _unregister_param_##_var() \
	{ \
		cmdline_unregister_param(&_param_##_var); \
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
void cmdline_print_module_options(char *module);

#endif

