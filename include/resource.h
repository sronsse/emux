#ifndef _RESOURCE_H
#define _RESOURCE_H

#include <stdint.h>

#define MEM_START(_name, _bus_id, _start, _end) \
	{ \
		.name = _name, \
		.data.mem = { \
			.bus_id = _bus_id, \
			.start = _start, \
			.end = _end \
		}, \
		.type = RESOURCE_MEM,
#define MEM_END \
	}

#define IRQ(_name, _irq) \
	{ \
		.name = _name, \
		.data.irq = _irq, \
		.type = RESOURCE_IRQ \
	}

#define CLK(_name, _clk) \
	{ \
		.name = _name, \
		.data.clk = _clk, \
		.type = RESOURCE_CLK \
	}

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
		uint64_t clk;
	} data;
	enum resource_type type;
	struct resource *children;
	int num_children;
};

struct resource *resource_get(char *name, enum resource_type type,
	struct resource *resources, int num_resources);

#endif

