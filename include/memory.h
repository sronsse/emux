#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdbool.h>
#include <stdint.h>
#include <list.h>
#include <resource.h>

#define KB(x) (x * 1024)
#define MB(x) (x * 1024 * 1024)

/* address_t size should match the maximum bus size of all supported machines */
typedef uint16_t address_t;

typedef void region_data_t;
typedef uint8_t (*readb_t)(region_data_t *data, address_t address);
typedef uint16_t (*readw_t)(region_data_t *data, address_t address);
typedef void (*writeb_t)(region_data_t *data, uint8_t b, address_t address);
typedef void (*writew_t)(region_data_t *data, uint16_t w, address_t address);

struct mops {
	readb_t readb;
	readw_t readw;
	writeb_t writeb;
	writew_t writew;
};

struct region {
	struct resource *area;
	struct mops *mops;
	region_data_t *data;
};

struct bus {
	int id;
	int width;
	struct list_link **readb_map;
	struct list_link **readw_map;
	struct list_link **writeb_map;
	struct list_link **writew_map;
	struct list_link *regions;
};

bool memory_bus_add(struct bus *bus);
void memory_bus_remove(struct bus *bus);
void memory_bus_remove_all();
bool memory_region_add(struct region *region);
void memory_region_remove(struct region *region);
uint8_t memory_readb(int bus_id, address_t address);
uint16_t memory_readw(int bus_id, address_t address);
void memory_writeb(int bus_id, uint8_t b, address_t address);
void memory_writew(int bus_id, uint16_t w, address_t address);

extern struct mops rom_mops;
extern struct mops ram_mops;

#endif

