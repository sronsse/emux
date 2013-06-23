#ifndef _RESOURCE_H
#define _RESOURCE_H

#include <stdint.h>

enum resource_type {
	RESOURCE_MEM,
	RESOURCE_IRQ,
	RESOURCE_CLK
};

struct resource {
	char *name;
	union {
		struct {
			int bus_id;
			uint16_t start;
			uint16_t end;
		} mem;
		int irq;
		uint64_t rate;
	};
	enum resource_type type;
	struct resource *children;
	int num_children;
};

struct resource *resource_get(char *name, enum resource_type type,
	struct resource *resources, int num_resources);

#endif

