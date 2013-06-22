#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>
#include <resource.h>

#define KB(x) (x * 1024)

typedef void region_data_t;

struct mops {
	uint8_t (*readb)(region_data_t *data, uint16_t address);
	uint16_t (*readw)(region_data_t *data, uint16_t address);
	void (*writeb)(region_data_t *data, uint8_t b, uint16_t address);
	void (*writew)(region_data_t *data, uint16_t w, uint16_t address);
};

struct region {
	struct resource *area;
	struct mops *mops;
	region_data_t *data;
};

void memory_region_add(struct region *region);
void memory_region_remove_all();
uint8_t memory_readb(int bus_id, uint16_t address);
uint16_t memory_readw(int bus_id, uint16_t address);
void memory_writeb(int bus_id, uint8_t b, uint16_t address);
void memory_writew(int bus_id, uint16_t w, uint16_t address);
void *memory_map_file(char *path, int offset, int size);
void memory_unmap_file(void *data, int size);

#endif

