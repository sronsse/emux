#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <bitops.h>
#include <log.h>
#include <memory.h>
#include <util.h>

static uint8_t rom_readb(uint8_t *rom, address_t address);
static uint16_t rom_readw(uint8_t *rom, address_t address);
static uint8_t ram_readb(uint8_t *ram, address_t address);
static uint16_t ram_readw(uint8_t *ram, address_t address);
static void ram_writeb(uint8_t *ram, uint8_t b, address_t address);
static void ram_writew(uint8_t *ram, uint16_t w, address_t address);
static struct bus *get_bus(int bus_id);
static void insert_region(struct bus *b, struct region *r, struct resource *a);
static void remove_region(struct bus *b, struct region *r, struct resource *a);
static bool fixup_address(struct region *region, address_t *address);

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
	struct list_link *bus_link = busses;
	struct bus *bus;

	/* Find bus with match bus ID */
	while ((bus = list_get_next(&bus_link)))
		if (bus->id == bus_id)
			return bus;

	/* No bus was found */
	return NULL;
}

bool memory_bus_add(struct bus *bus)
{
	int size;

	/* Verify bus ID is not already present */
	if (get_bus(bus->id)) {
		LOG_D("Bus %u was already added!\n", bus->id);
		return false;
	}

	/* Initialize bus */
	bus->regions = NULL;
	size = BIT(bus->width) * sizeof(struct list_link *);
	bus->readb_map = malloc(size);
	bus->readw_map = malloc(size);
	bus->writeb_map = malloc(size);
	bus->writew_map = malloc(size);
	memset(bus->readb_map, 0, size);
	memset(bus->readw_map, 0, size);
	memset(bus->writeb_map, 0, size);
	memset(bus->writew_map, 0, size);

	/* Add bus to list */
	list_insert(&busses, bus);

	return true;
}

void memory_bus_remove(struct bus *bus)
{
	int size;
	int i;

	/* Free map lists */
	size = BIT(bus->width);
	for (i = 0; i < size; i++) {
		list_remove_all(&bus->readb_map[i]);
		list_remove_all(&bus->readw_map[i]);
		list_remove_all(&bus->writeb_map[i]);
		list_remove_all(&bus->writew_map[i]);
	}

	/* Free maps */
	free(bus->readb_map);
	free(bus->readw_map);
	free(bus->writeb_map);
	free(bus->writew_map);

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

void insert_region(struct bus *b, struct region *r, struct resource *a)
{
	int start = a->data.mem.start;
	int end = a->data.mem.end;
	int i;

	/* Insert region into bus maps */
	for (i = start; i <= end; i++) {
		if (r->mops->readb)
			list_insert_before(&b->readb_map[i], r);
		if (r->mops->readw)
			list_insert_before(&b->readw_map[i], r);
		if (r->mops->writeb)
			list_insert_before(&b->writeb_map[i], r);
		if (r->mops->writew)
			list_insert_before(&b->writew_map[i], r);
	}
}

void remove_region(struct bus *b, struct region *r, struct resource *a)
{
	int start = a->data.mem.start;
	int end = a->data.mem.end;
	int i;

	/* Remove region from bus maps */
	for (i = start; i <= end; i++) {
		list_remove(&b->readb_map[i], r);
		list_remove(&b->readw_map[i], r);
		list_remove(&b->writeb_map[i], r);
		list_remove(&b->writew_map[i], r);
	}
}

bool memory_region_add(struct region *region)
{
	struct bus *bus;
	int bus_id;
	int i;

	/* Get bus based on area and return if not found */
	bus_id = region->area->data.mem.bus_id;
	bus = get_bus(bus_id);
	if (!bus) {
		LOG_D("Bus %u was not found!\n", bus_id);
		return false;
	}

	/* Insert region */
	list_insert(&bus->regions, region);

	/* Fill bus maps for region area and its mirrors */
	insert_region(bus, region, region->area);
	for (i = 0; i < region->area->num_children; i++)
		insert_region(bus, region, &region->area->children[i]);

	return true;
}

void memory_region_remove(struct region *region)
{
	struct bus *bus = NULL;
	int bus_id;
	int i;

	/* Get bus based on area and return if not found */
	bus_id = region->area->data.mem.bus_id;
	bus = get_bus(bus_id);
	if (!bus) {
		LOG_D("Bus %u was not found!\n", bus_id);
		return;
	}

	/* Remove region from bus maps */
	remove_region(bus, region, region->area);
	for (i = 0; i < region->area->num_children; i++)
		remove_region(bus, region, &region->area->children[i]);

	/* Remove region from list */
	list_remove(&bus->regions, region);
}

bool fixup_address(struct region *region, address_t *address)
{
	address_t start;
	address_t end;
	int i;

	/* Check main area first */
	start = region->area->data.mem.start;
	end = region->area->data.mem.end;
	if ((*address >= start) && (*address <= end)) {
		*address -= start;
		return true;
	}

	/* Check mirrors next */
	for (i = 0; i < region->area->num_children; i++) {
		start = region->area->children[i].data.mem.start;
		end = region->area->children[i].data.mem.end;
		if ((*address >= start) && (*address <= end)) {
			*address -= start;
			return true;
		}
	}

	/* Address could not be fixed up */
	return false;
}

uint8_t memory_readb(int bus_id, address_t address)
{
	struct bus *bus;
	struct region *region;
	struct list_link *map;

	/* Get bus */
	bus = get_bus(bus_id);
	if (!bus) {
		LOG_W("Bus %u does not exist!\n", bus_id);
		return 0;
	}

	/* Get region */
	map = bus->readb_map[address];
	region = list_get_next(&map);
	if (!region) {
		LOG_W("Region not found (readb %u, %04x)!\n", bus_id, address);
		return 0;
	}

	/* Adapt address */
	if (!fixup_address(region, &address)) {
		LOG_E("Address %04x fixup (bus %u) failed!\n", address, bus_id);
		return 0;
	}

	/* Call memory operation */
	return region->mops->readb(region->data, address);
}

uint16_t memory_readw(int bus_id, address_t address)
{
	struct bus *bus;
	struct region *region;
	struct list_link *map;

	/* Get bus */
	bus = get_bus(bus_id);
	if (!bus) {
		LOG_W("Bus %u does not exist!\n", bus_id);
		return 0;
	}

	/* Get region */
	map = bus->readw_map[address];
	region = list_get_next(&map);
	if (!region) {
		LOG_W("Region not found (readw %u, %04x)!\n", bus_id, address);
		return 0;
	}

	/* Adapt address */
	if (!fixup_address(region, &address)) {
		LOG_E("Address %04x fixup (bus %u) failed!\n", address, bus_id);
		return 0;
	}

	/* Call memory operation */
	return region->mops->readw(region->data, address);
}

void memory_writeb(int bus_id, uint8_t b, address_t address)
{
	struct bus *bus;
	struct region *region;
	struct list_link *map;

	/* Get bus */
	bus = get_bus(bus_id);
	if (!bus) {
		LOG_W("Bus %u does not exist!\n", bus_id);
		return;
	}

	/* Get region */
	map = bus->writeb_map[address];
	region = list_get_next(&map);
	if (!region) {
		LOG_W("Region not found (writeb %u, %04x)!\n", bus_id, address);
		return;
	}

	/* Adapt address */
	if (!fixup_address(region, &address)) {
		LOG_E("Address %04x fixup (bus %u) failed!\n", address, bus_id);
		return;
	}

	/* Call memory operation */
	region->mops->writeb(region->data, b, address);
}

void memory_writew(int bus_id, uint16_t w, address_t address)
{
	struct bus *bus;
	struct region *region;
	struct list_link *map;

	/* Get bus */
	bus = get_bus(bus_id);
	if (!bus) {
		LOG_W("Bus %u does not exist!\n", bus_id);
		return;
	}

	/* Get region */
	map = bus->writew_map[address];
	region = list_get_next(&map);
	if (!region) {
		LOG_W("Region not found (writew %u, %04x)!\n", bus_id, address);
		return;
	}

	/* Adapt address */
	if (!fixup_address(region, &address)) {
		LOG_E("Address %04x fixup (bus %u) failed!\n", address, bus_id);
		return;
	}

	/* Call memory operation */
	region->mops->writew(region->data, w, address);
}

