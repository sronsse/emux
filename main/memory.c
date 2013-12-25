#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#endif
#include <bitops.h>
#include <log.h>
#include <memory.h>
#include <util.h>

struct bus {
	int width;
	struct region *regions;
	int num_regions;
};

struct region {
	address_t start;
	address_t end;
	struct mops *mops;
	region_data_t *data;
};

static uint8_t rom_readb(region_data_t *data, address_t address);
static uint16_t rom_readw(region_data_t *data, address_t address);
static uint8_t ram_readb(region_data_t *data, address_t address);
static uint16_t ram_readw(region_data_t *data, address_t address);
static void ram_writeb(region_data_t *data, uint8_t b, address_t address);
static void ram_writew(region_data_t *data, uint16_t w, address_t address);
static int memory_region_sort_compare(const void *a, const void *b);
static int memory_region_bsearch_compare(const void *key, const void *elem);
static struct region *memory_region_find(int bus_id, address_t *address);

static struct bus *busses;
static int num_busses;

struct mops rom_mops = {
	.readb = rom_readb,
	.readw = rom_readw
};

struct mops ram_mops = {
	.readb = ram_readb,
	.readw = ram_readw,
	.writeb = ram_writeb,
	.writew = ram_writew
};

uint8_t rom_readb(region_data_t *data, address_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	return *mem;
}

uint16_t rom_readw(region_data_t *data, address_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	return (*(mem + 1) << 8) | *mem;
}

uint8_t ram_readb(region_data_t *data, address_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	return *mem;
}

uint16_t ram_readw(region_data_t *data, address_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	return (*(mem + 1) << 8) | *mem;
}

void ram_writeb(region_data_t *data, uint8_t b, address_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	*mem = b;
}

void ram_writew(region_data_t *data, uint16_t w, address_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
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

struct region *memory_region_find(int bus_id, address_t *address)
{
	struct region *region;

	/* Make sure address fits within bus */
	*address &= (BIT(busses[bus_id].width) - 1);

	/* Search region */
	region = bsearch(address,
		busses[bus_id].regions,
		busses[bus_id].num_regions,
		sizeof(struct region),
		memory_region_bsearch_compare);

	/* Adapt address if a region was found */
	if (region)
		*address -= region->start;

	return region;
}

void memory_bus_add(int width)
{
	/* Grow busses array */
	busses = realloc(busses, ++num_busses * sizeof(struct bus));

	/* Initialize bus */
	busses[num_busses - 1].width = width;
	busses[num_busses - 1].regions = NULL;
	busses[num_busses - 1].num_regions = 0;
}

void memory_region_add(struct resource *area, struct mops *mops,
	region_data_t *data)
{
	struct bus *bus;
	struct region *region;
	int i;

	/* Get bus based on area */
	bus = &busses[area->data.mem.bus_id];

	/* Grow memory regions array */
	bus->regions = realloc(bus->regions, ++bus->num_regions *
		sizeof(struct region));

	/* Create memory region */
	region = &bus->regions[bus->num_regions - 1];
	region->start = area->data.mem.start & (BIT(bus->width) - 1);
	region->end = area->data.mem.end & (BIT(bus->width) - 1);
	region->mops = mops;
	region->data = data;

	/* Add mirrors */
	for (i = 0; i < area->num_children; i++)
		memory_region_add(&area->children[i], mops, data);

	/* Sort memory regions array */
	qsort(bus->regions, bus->num_regions, sizeof(struct region),
		memory_region_sort_compare);
}

void memory_region_remove(struct resource *area)
{
	struct bus *bus;
	address_t start;
	address_t end;
	int index;

	/* Get bus based on area */
	bus = &busses[area->data.mem.bus_id];

	/* Get region start and end addresses */
	start = area->data.mem.start;
	end = area->data.mem.end;

	/* Find region to remove */
	for (index = 0; index < bus->num_regions; index++)
		if ((bus->regions[index].start == start) &&
			(bus->regions[index].end == end))
			break;

	/* Return if region was not found */
	if (index == bus->num_regions)
		return;

	/* Shift remaining regions */
	while (index < bus->num_regions - 1) {
		bus->regions[index] = bus->regions[index + 1];
		index++;
	}

	/* Shrink memory regions array */
	bus->regions = realloc(bus->regions, --bus->num_regions *
		sizeof(struct region));
}

void memory_bus_remove_all()
{
	int i;
	for (i = 0; i < num_busses; i++)
		free(busses[i].regions);
	free(busses);
	busses = NULL;
	num_busses = 0;
}

uint8_t memory_readb(int bus_id, address_t address)
{
	struct region *region = memory_region_find(bus_id, &address);

	if (region && region->mops->readb)
		return region->mops->readb(region->data, address);

	LOG_W("No region found at (%u, %04x)!\n", bus_id, address);
	return 0;
}

uint16_t memory_readw(int bus_id, address_t address)
{
	struct region *region = memory_region_find(bus_id, &address);

	if (region && region->mops->readw)
		return region->mops->readw(region->data, address);

	LOG_W("No region found at (%u, %04x)!\n", bus_id, address);
	return 0;
}

void memory_writeb(int bus_id, uint8_t b, address_t address)
{
	struct region *region = memory_region_find(bus_id, &address);

	if (region && region->mops->writeb) {
		region->mops->writeb(region->data, b, address);
		return;
	}

	LOG_W("No region found at (%u, %04x)!\n", bus_id, address);
}

void memory_writew(int bus_id, uint16_t w, address_t address)
{
	struct region *region = memory_region_find(bus_id, &address);

	if (region && region->mops->writew) {
		region->mops->writew(region->data, w, address);
		return;
	}

	LOG_W("No region found at (%u, %04x)!\n", bus_id, address);
}

void *memory_map_file(char *path, int offset, int size)
{
#ifdef _WIN32
	SYSTEM_INFO system_info;
	HANDLE file;
	HANDLE mapping;
	char *data;
	int pa_offset;

	GetSystemInfo(&system_info);
	pa_offset = offset & ~(system_info.dwAllocationGranularity - 1);
	size += offset - pa_offset;

	file = CreateFile(path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if (file == INVALID_HANDLE_VALUE)
		return NULL;

	mapping = CreateFileMapping(file, 0, PAGE_READONLY, 0, size, 0);
	CloseHandle(file);
	if (!mapping)
		return NULL;

	data = MapViewOfFile(mapping, FILE_MAP_READ, 0, pa_offset, size);
	CloseHandle(mapping);

	data += offset - pa_offset;
	return data;
#else
	int fd;
	struct stat sb;
	char *data;
	int pa_offset;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return NULL;

	fstat(fd, &sb);
	if (!S_ISREG(sb.st_mode)) {
		close(fd);
		return NULL;
	}

	if (offset + size > sb.st_size) {
		close(fd);
		return NULL;
	}

	/* Offset for mmap must be page-aligned */
	pa_offset = offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
	size += offset - pa_offset;

	data = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, pa_offset);
	if (data == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	/* Adapt returned pointer to point to requested location */
	data += offset - pa_offset;

	close(fd);
	return data;
#endif
}

void memory_unmap_file(void *data, int size)
{
#ifdef _WIN32
	SYSTEM_INFO system_info;
	int pa_data;
	GetSystemInfo(&system_info);
	pa_data = (int)data & ~(system_info.dwPageSize - 1);
	size += (int)data - pa_data;
	UnmapViewOfFile((void *)pa_data);
#else
	intptr_t pa_data = (intptr_t)data & ~(sysconf(_SC_PAGE_SIZE) - 1);
	size += (intptr_t)data - pa_data;
	munmap((void *)pa_data, size);
#endif
}

