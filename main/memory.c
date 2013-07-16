#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#endif
#include <list.h>
#include <memory.h>
#include <util.h>

#define WITHIN_REGION(bus_id, address, region) \
	((bus_id == region->bus_id) && \
		(address >= region->start) && \
		(address <= region->end))

struct region {
	int bus_id;
	uint16_t start;
	uint16_t end;
	struct mops *mops;
	region_data_t *data;
};

static uint8_t rom_readb(region_data_t *data, uint16_t address);
static uint16_t rom_readw(region_data_t *data, uint16_t address);
static uint8_t ram_readb(region_data_t *data, uint16_t address);
static uint16_t ram_readw(region_data_t *data, uint16_t address);
static void ram_writeb(region_data_t *data, uint8_t b, uint16_t address);
static void ram_writew(region_data_t *data, uint16_t w, uint16_t address);
static struct region *memory_find_region(struct list_link **regions, int bus_id,
	uint16_t *a);

static struct list_link *memory_regions;

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

uint8_t rom_readb(region_data_t *data, uint16_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	return *mem;
}

uint16_t rom_readw(region_data_t *data, uint16_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	return (*(mem + 1) << 8) | *mem;
}

uint8_t ram_readb(region_data_t *data, uint16_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	return *mem;
}

uint16_t ram_readw(region_data_t *data, uint16_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	return (*(mem + 1) << 8) | *mem;
}

void ram_writeb(region_data_t *data, uint8_t b, uint16_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	*mem = b;
}

void ram_writew(region_data_t *data, uint16_t w, uint16_t address)
{
	uint8_t *mem = (uint8_t *)data + address;
	*mem++ = w;
	*mem = w >> 8;
}

struct region *memory_find_region(struct list_link **regions, int bus_id,
	uint16_t *a)
{
	struct region *region;
	while ((region = list_get_next(regions)))
		if (WITHIN_REGION(bus_id, *a, region)) {
			*a -= region->start;
			return region;
		}

	/* No region was found */
	return NULL;
}

void memory_region_add(struct resource *area, struct mops *mops,
	region_data_t *data)
{
	struct region *region;
	struct resource *mirror;
	int i;

	/* Create main memory region */
	region = malloc(sizeof(struct region));
	region->bus_id = area->data.mem.bus_id;
	region->start = area->data.mem.start;
	region->end = area->data.mem.end;
	region->mops = mops;
	region->data = data;
	list_insert(&memory_regions, region);

	/* Create mirrors */
	for (i = 0; i < area->num_children; i++) {
		mirror = &area->children[i];
		region = malloc(sizeof(struct region));
		region->bus_id = mirror->data.mem.bus_id;
		region->start = mirror->data.mem.start;
		region->end = mirror->data.mem.end;
		region->mops = mops;
		region->data = data;
		list_insert(&memory_regions, region);
	}
}

void memory_region_remove_all()
{
	struct list_link *link = memory_regions;
	struct region *region;

	/* Free all regions */
	while ((region = list_get_next(&link)))
		free(region);

	/* Free list */
	list_remove_all(&memory_regions);
}

uint8_t memory_readb(int bus_id, uint16_t address)
{
	struct list_link *regions = memory_regions;
	struct region *region;
	uint16_t a = address;

	while ((region = memory_find_region(&regions, bus_id, &a))) {
		if (region->mops->readb)
			return region->mops->readb(region->data, a);
		a = address;
	}

	fprintf(stderr, "Error: no region found at (%u, %04x)!\n",
		bus_id, address);
	return 0;
}

uint16_t memory_readw(int bus_id, uint16_t address)
{
	struct list_link *regions = memory_regions;
	struct region *region;
	uint16_t a = address;

	while ((region = memory_find_region(&regions, bus_id, &a))) {
		if (region->mops->readw)
			return region->mops->readw(region->data, a);
		a = address;
	}

	fprintf(stderr, "Error: no region found at (%u, %04x)!\n",
		bus_id, address);
	return 0;
}

void memory_writeb(int bus_id, uint8_t b, uint16_t address)
{
	struct list_link *regions = memory_regions;
	struct region *region;
	uint16_t a = address;
	bool found = false;

	while ((region = memory_find_region(&regions, bus_id, &a))) {
		if (region->mops->writeb)
			region->mops->writeb(region->data, b, a);
		a = address;
		found = true;
	}

	if (!found)
		fprintf(stderr, "Error: no region found at (%u, %04x)!\n",
			bus_id, address);
}

void memory_writew(int bus_id, uint16_t w, uint16_t address)
{
	struct list_link *regions = memory_regions;
	struct region *region;
	uint16_t a = address;
	bool found = false;

	while ((region = memory_find_region(&regions, bus_id, &a))) {
		if (region->mops->writew)
			region->mops->writew(region->data, w, a);
		a = address;
		found = true;
	}

	if (!found)
		fprintf(stderr, "Error: no region found at (%u, %04x)!\n",
			bus_id, address);
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
	pa_offset = offset & ~(system_info.dwPageSize - 1);
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

