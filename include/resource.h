#ifndef _RESOURCE_H
#define _RESOURCE_H

#include <stdint.h>

#define MEMX(_name, _bus_id, _start, _end, _children, _num_children) \
	{ \
		.name = _name, \
		.data.mem = { \
			.bus_id = _bus_id, \
			.start = _start, \
			.end = _end \
		}, \
		.type = RESOURCE_MEM, \
		.children = _children, \
		.num_children = _num_children \
	}
#define MEM(_name, _bus_id, _start, _end) \
	MEMX(_name, _bus_id, _start, _end, NULL, 0)

#define DMA(_name, _channel) \
	{ \
		.name = _name, \
		.data.dma = { \
			.channel = _channel, \
		}, \
		.type = RESOURCE_DMA \
	}

#define PORTX(_name, _start, _end, _children, _num_children) \
	{ \
		.name = _name, \
		.data.port = { \
			.start = _start, \
			.end = _end \
		}, \
		.type = RESOURCE_PORT, \
		.children = _children, \
		.num_children = _num_children \
	}
#define PORT(_name, _start, _end) \
	PORTX(_name, _start, _end, NULL, 0)

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
	RESOURCE_DMA,
	RESOURCE_PORT,
	RESOURCE_IRQ,
	RESOURCE_CLK
};

struct resource {
	char *name;
	union {
		struct {
			int bus_id;
			uint32_t start;
			uint32_t end;
		} mem;
		struct {
			int channel;
		} dma;
		struct {
			uint8_t start;
			uint8_t end;
		} port;
		int irq;
		float clk;
	} data;
	enum resource_type type;
	struct resource *children;
	int num_children;
};

struct resource *resource_get(char *name, enum resource_type type,
	struct resource *resources, int num_resources);

#endif

