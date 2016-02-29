#ifndef _PORT_H
#define _PORT_H

#include <stdint.h>
#include <list.h>
#include <resource.h>

#define PORT_SIZE(area) \
	(area->data.port.end - area->data.port.start + 1)

typedef uint8_t port_t;

typedef void port_data_t;
typedef uint8_t (*read_t)(port_data_t *data, port_t port);
typedef void (*write_t)(port_data_t *data, uint8_t b, port_t port);

struct pops {
	read_t read;
	write_t write;
};

struct port_region {
	struct resource *area;
	struct pops *pops;
	port_data_t *data;
};

bool port_region_add(struct port_region *region);
void port_region_remove(struct port_region *region);
void port_region_remove_all();
uint8_t port_read(port_t port);
void port_write(uint8_t b, port_t port);

#endif

