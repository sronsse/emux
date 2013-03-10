#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>
#include <resource.h>

#define KB(x) (x * 1024)

struct region;

struct mops {
	uint8_t (*readb)(struct region *region, uint16_t address);
	uint16_t (*readw)(struct region *region, uint16_t address);
	void (*writeb)(struct region *region, uint8_t b, uint16_t address);
	void (*writew)(struct region *region, uint16_t w, uint16_t address);
};

struct region {
	struct resource *area;
	struct resource *mirrors;
	int num_mirrors;
	struct mops *mops;
	void *data;
};

struct region_link {
	struct region *region;
	struct region_link *next;
};

void memory_region_add(struct region *region);
void memory_region_remove_all();
uint8_t memory_readb(uint16_t address);
uint16_t memory_readw(uint16_t address);
void memory_writeb(uint8_t b, uint16_t address);
void memory_writew(uint16_t w, uint16_t address);
void *memory_map_file(char *path, int offset, int size);
void memory_unmap_file(void *data, int size);

#endif

