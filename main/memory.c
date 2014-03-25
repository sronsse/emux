#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <bitops.h>
#include <log.h>
#include <memory.h>
#include <util.h>

#define USE_BUS_MAP

struct bus {
	int width;
	struct region *regions;
	int num_regions;
#ifdef USE_BUS_MAP
	struct region **map;
#endif
};

struct region {
	address_t start;
	address_t end;
	struct mops *mops;
	region_data_t *data;
};

static uint8_t rom_readb(uint8_t *rom, address_t address);
static uint16_t rom_readw(uint8_t *rom, address_t address);
static uint8_t ram_readb(uint8_t *ram, address_t address);
static uint16_t ram_readw(uint8_t *ram, address_t address);
static void ram_writeb(uint8_t *ram, uint8_t b, address_t address);
static void ram_writew(uint8_t *ram, uint16_t w, address_t address);
static int memory_region_sort_compare(const void *a, const void *b);
#ifndef USE_BUS_MAP
static int memory_region_bsearch_compare(const void *key, const void *elem);
#endif
static struct region *memory_region_find(int bus_id, address_t *address);

static struct bus *busses;
static int num_busses;

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

int memory_region_sort_compare(const void *a, const void *b)
{
	struct region *r1 = (struct region *)a;
	struct region *r2 = (struct region *)b;

	/* Check start address next */
	if (r1->start < r2->start)
		return -1;
	else if (r1->start > r2->start)
		return 1;

	/* Bus conflict (this case should not happen) */
	return 0;
}

#ifndef USE_BUS_MAP
int memory_region_bsearch_compare(const void *key, const void *elem)
{
	address_t address = *(address_t *)key;
	struct region *r = (struct region *)elem;

	/* Check address next */
	if (address < r->start)
		return -1;
	else if (address > r->end)
		return 1;

	/* Region matches */
	return 0;
}
#endif

struct region *memory_region_find(int bus_id, address_t *address)
{
	struct region *region;

	/* Make sure address fits within bus */
	*address &= (BIT(busses[bus_id].width) - 1);

#ifdef USE_BUS_MAP
	/* Get region from bus map */
	region = busses[bus_id].map[*address];
#else
	/* Search region */
	region = bsearch(address,
		busses[bus_id].regions,
		busses[bus_id].num_regions,
		sizeof(struct region),
		memory_region_bsearch_compare);
#endif

	/* Adapt address if a region was found */
	if (region)
		*address -= region->start;

	return region;
}

void memory_bus_add(int width)
{
	struct bus *bus;
#ifdef USE_BUS_MAP
	int size;
#endif

	/* Grow busses array and select bus */
	busses = realloc(busses, ++num_busses * sizeof(struct bus));
	bus = &busses[num_busses - 1];

	/* Initialize bus */
	bus->width = width;
	bus->regions = NULL;
	bus->num_regions = 0;

#ifdef USE_BUS_MAP
	/* Initialize bus map */
	size = BIT(width) * sizeof(struct region *);
	bus->map = malloc(size);
	memset(bus->map, 0, size);
#endif
}

void memory_region_add(struct resource *a, struct mops *m, region_data_t *d)
{
	struct bus *bus;
	struct region *region;
	int i;
#ifdef USE_BUS_MAP
	int j;
#endif

	/* Get bus based on area */
	bus = &busses[a->data.mem.bus_id];

	/* Grow memory regions array */
	bus->regions = realloc(bus->regions, ++bus->num_regions *
		sizeof(struct region));

	/* Create memory region */
	region = &bus->regions[bus->num_regions - 1];
	region->start = a->data.mem.start & (BIT(bus->width) - 1);
	region->end = a->data.mem.end & (BIT(bus->width) - 1);
	region->mops = m;
	region->data = d;

	/* Sort memory regions array */
	qsort(bus->regions, bus->num_regions, sizeof(struct region),
		memory_region_sort_compare);

	/* Add mirrors */
	for (i = 0; i < a->num_children; i++)
		memory_region_add(&a->children[i], m, d);

#ifdef USE_BUS_MAP
	/* Update bus map */
	for (i = 0; i < bus->num_regions; i++)
		for (j = bus->regions[i].start; j <= bus->regions[i].end; j++)
			bus->map[j] = &bus->regions[i];
#endif
}

void memory_region_remove(struct resource *area)
{
	struct bus *bus;
	address_t start;
	address_t end;
#ifdef USE_BUS_MAP
	int size;
#endif
	int i;

	/* Get bus based on area */
	bus = &busses[area->data.mem.bus_id];

	/* Get region start and end addresses */
	start = area->data.mem.start;
	end = area->data.mem.end;

	/* Find region to remove */
	for (i = 0; i < bus->num_regions; i++)
		if ((bus->regions[i].start == start) &&
			(bus->regions[i].end == end))
			break;

	/* Return if region was not found */
	if (i == bus->num_regions)
		return;

#ifdef USE_BUS_MAP
	/* Reset bus map */
	size = (end - start + 1) * sizeof(struct region *);
	memset(&bus->map[start], 0, size);
#endif

	/* Shift remaining regions */
	while (i < bus->num_regions - 1) {
		bus->regions[i] = bus->regions[i + 1];
		i++;
	}

	/* Shrink memory regions array */
	bus->regions = realloc(bus->regions, --bus->num_regions *
		sizeof(struct region));

	/* Remove mirrors */
	for (i = 0; i < area->num_children; i++)
		memory_region_remove(&area->children[i]);
}

void memory_bus_remove_all()
{
	int i;

	/* Parse busses */
	for (i = 0; i < num_busses; i++) {
		/* Free regions */
		free(busses[i].regions);

#ifdef USE_BUS_MAP
		/* Free map */
		free(busses[i].map);
#endif
	}

	/* Free busses */
	free(busses);
	busses = NULL;
	num_busses = 0;
}

uint8_t memory_readb(int bus_id, address_t address)
{
	struct region *region = memory_region_find(bus_id, &address);

	if (region && region->mops->readb)
		return region->mops->readb(region->data, address);

	LOG_W("No region found at (readb %u, %04x)!\n", bus_id, address);
	return 0;
}

uint16_t memory_readw(int bus_id, address_t address)
{
	struct region *region = memory_region_find(bus_id, &address);

	if (region && region->mops->readw)
		return region->mops->readw(region->data, address);

	LOG_W("No region found at (readw %u, %04x)!\n", bus_id, address);
	return 0;
}

void memory_writeb(int bus_id, uint8_t b, address_t address)
{
	struct region *region = memory_region_find(bus_id, &address);

	if (region && region->mops->writeb) {
		region->mops->writeb(region->data, b, address);
		return;
	}

	LOG_W("No region found at (writeb %u, %04x)!\n", bus_id, address);
}

void memory_writew(int bus_id, uint16_t w, address_t address)
{
	struct region *region = memory_region_find(bus_id, &address);

	if (region && region->mops->writew) {
		region->mops->writew(region->data, w, address);
		return;
	}

	LOG_W("No region found at (writew %u, %04x)!\n", bus_id, address);
}

