#ifndef _RESOURCE_H
#define _RESOURCE_H

#include <stdint.h>

enum resource_type {
	RESOURCE_MEM,
	RESOURCE_CLK
};

struct resource {
	char *name;
	uint64_t start;
	uint64_t end;
	enum resource_type type;
	struct resource *children;
	int num_children;
};

struct resource *resource_get(char *name, enum resource_type type,
	struct resource *resources, int num_resources);

#endif

