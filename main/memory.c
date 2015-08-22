#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <bitops.h>
#include <log.h>
#include <memory.h>
#include <util.h>

#define DEFINE_MEMORY_READ(ext, type) \
	type memory_read##ext(int bus_id, address_t address) \
	{ \
		struct bus *bus; \
		struct region *r; \
		struct resource *mirror; \
		struct list_link *link; \
		address_t a; \
		int i; \
	\
		/* Get bus and return 0 on failure */ \
		bus = get_bus(bus_id); \
		if (!bus) { \
			LOG_W("Bus not found (%s(%u, %08x))!\n", \
				__func__, \
				bus_id, \
				address); \
			return 0; \
		} \
	\
		/* Parse regions */ \
		link = bus->regions; \
		while ((r = list_get_next(&link))) { \
			/* Skip if region if operation is not supported */ \
			if (!r->mops->read##ext) \
				continue; \
	\
			/* Call operation if address is within area */ \
			if ((address >= r->area->data.mem.start) && \
				(address <= r->area->data.mem.end)) { \
				a = address - r->area->data.mem.start; \
				return r->mops->read##ext(r->data, a); \
			} \
	\
			/* Call operation if address is within a mirror */ \
			for (i = 0; i < r->area->num_children; i++) { \
				mirror = &r->area->children[i]; \
				if ((address >= mirror->data.mem.start) && \
					(address <= mirror->data.mem.end)) { \
					a = address - mirror->data.mem.start; \
					return r->mops->read##ext(r->data, a); \
				} \
			} \
		} \
	\
		/* Return 0 in case of read failure */ \
		LOG_W("Region not found (%s(%u, %08x))!\n", \
			__func__, \
			bus_id, \
			address); \
		return 0; \
	}

#define DEFINE_MEMORY_WRITE(ext, type) \
	void memory_write##ext(int bus_id, type data, address_t address) \
	{ \
		struct bus *bus; \
		struct region *r; \
		struct resource *mirror; \
		struct list_link *link; \
		struct region **regions; \
		address_t *addresses; \
		address_t a; \
		int num; \
		int i; \
	\
		/* Get bus and return on failure */ \
		bus = get_bus(bus_id); \
		if (!bus) { \
			LOG_W("Bus not found (%s(%u, %08x))!\n", \
				__func__, \
				bus_id, \
				address); \
			return; \
		} \
	\
		/* Parse regions */ \
		link = bus->regions; \
		regions = NULL; \
		addresses = NULL; \
		num = 0; \
		while ((r = list_get_next(&link))) { \
			/* Skip if region if operation is not supported */ \
			if (!r->mops->write##ext) \
				continue; \
	\
			/* Add region and adapted address if needed */ \
			if ((address >= r->area->data.mem.start) && \
				(address <= r->area->data.mem.end)) { \
				a = address - r->area->data.mem.start; \
				num++; \
				regions = realloc(regions, \
					num * sizeof(struct region *)); \
				addresses = realloc(addresses, \
					num * sizeof(address_t)); \
				regions[num - 1] = r; \
				addresses[num - 1] = a; \
			} \
	\
			/* Parse mirrors */ \
			for (i = 0; i < r->area->num_children; i++) { \
				mirror = &r->area->children[i]; \
	\
				/* Skip if address is not within mirror */ \
				if (!((address >= mirror->data.mem.start) && \
					(address <= mirror->data.mem.end))) \
					continue; \
	\
				/* Add region and adapted address */ \
				a = address - mirror->data.mem.start; \
				num++; \
				regions = realloc(regions, \
					num * sizeof(struct region *)); \
				addresses = realloc(addresses, \
					num * sizeof(address_t)); \
				regions[num - 1] = r; \
				addresses[num - 1] = a; \
			} \
		} \
	\
		/* Call write operations */ \
		for (i = 0; i < num; i++) \
			regions[i]->mops->write##ext(regions[i]->data, \
				data, \
				addresses[i]); \
	\
		/* Warn on write failure */ \
		if (num == 0) \
			LOG_W("Region not found (%s(%u, %08x))!\n", \
				__func__, \
				bus_id, \
				address); \
	\
		/* Free arrays */ \
		free(regions); \
		free(addresses); \
	}

static uint8_t rom_readb(uint8_t *rom, address_t address);
static uint16_t rom_readw(uint8_t *rom, address_t address);
static uint8_t ram_readb(uint8_t *ram, address_t address);
static uint16_t ram_readw(uint8_t *ram, address_t address);
static void ram_writeb(uint8_t *ram, uint8_t b, address_t address);
static void ram_writew(uint8_t *ram, uint16_t w, address_t address);
static struct bus *get_bus(int bus_id);

static struct list_link *busses;

struct mops rom_mops = {
	.readb = (readb_t)rom_readb,
	.readw = (readw_t)rom_readw
};

struct mops ram_mops = {
	.readb = (readb_t)ram_readb,
	.readw = (readw_t)ram_readw,
	.writeb = (writeb_t)ram_writeb,
	.writew = (writew_t)ram_writew
};

uint8_t rom_readb(uint8_t *rom, address_t address)
{
	return rom[address];
}

uint16_t rom_readw(uint8_t *rom, address_t address)
{
	uint8_t *mem = rom + address;
	return (*(mem + 1) << 8) | *mem;
}

uint8_t ram_readb(uint8_t *ram, address_t address)
{
	return ram[address];
}

uint16_t ram_readw(uint8_t *ram, address_t address)
{
	uint8_t *mem = ram + address;
	return (*(mem + 1) << 8) | *mem;
}

void ram_writeb(uint8_t *ram, uint8_t b, address_t address)
{
	ram[address] = b;
}

void ram_writew(uint8_t *ram, uint16_t w, address_t address)
{
	uint8_t *mem = ram + address;
	*mem++ = w;
	*mem = w >> 8;
}

struct bus *get_bus(int bus_id)
{
	struct list_link *link = busses;
	struct bus *bus;

	/* Find bus with match bus ID */
	while ((bus = list_get_next(&link)))
		if (bus->id == bus_id)
			return bus;

	/* No bus was found */
	return NULL;
}

bool memory_bus_add(struct bus *bus)
{
	/* Verify bus ID is not already present */
	if (get_bus(bus->id)) {
		LOG_D("Bus %u was already added!\n", bus->id);
		return false;
	}

	/* Initialize bus */
	bus->regions = NULL;

	/* Add bus to list */
	list_insert(&busses, bus);

	return true;
}

void memory_bus_remove(struct bus *bus)
{
	/* Remove regions */
	list_remove_all(&bus->regions);

	/* Remove bus from list */
	list_remove(&busses, bus);
}

void memory_bus_remove_all()
{
	struct list_link *link = busses;
	struct bus *bus;

	while ((bus = list_get_next(&link)))
		memory_bus_remove(bus);
}

bool memory_region_add(struct region *region)
{
	struct bus *bus;
	int bus_id;

	/* Get bus based on area and return if not found */
	bus_id = region->area->data.mem.bus_id;
	bus = get_bus(bus_id);
	if (!bus) {
		LOG_D("Bus %u was not found!\n", bus_id);
		return false;
	}

	/* Insert region before others (it will take precedence on read ops) */
	list_insert_before(&bus->regions, region);

	return true;
}

void memory_region_remove(struct region *region)
{
	struct bus *bus = NULL;
	int bus_id;

	/* Get bus based on area and return if not found */
	bus_id = region->area->data.mem.bus_id;
	bus = get_bus(bus_id);
	if (!bus) {
		LOG_D("Bus %u was not found!\n", bus_id);
		return;
	}

	/* Remove region from list */
	list_remove(&bus->regions, region);
}

/* Define memory read/write functions */
DEFINE_MEMORY_READ(b, uint8_t)
DEFINE_MEMORY_READ(w, uint16_t)
DEFINE_MEMORY_WRITE(b, uint8_t)
DEFINE_MEMORY_WRITE(w, uint16_t)

