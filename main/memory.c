#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <machine.h>
#include <memory.h>

static struct region *memory_find_region(struct list_link **regions,
	uint16_t *a);

extern struct machine *machine;

struct region *memory_find_region(struct list_link **regions, uint16_t *a)
{
	struct region *r;
	struct region *region = NULL;
	struct resource *area;
	struct resource *mirror;
	int i;

	while ((r = list_get_next(regions))) {
		/* Check region area */
		area = r->area;
		if ((*a >= area->start) && (*a <= area->end)) {
			*a -= area->start;
			region = r;
		} else {
			/* Check region mirrors */
			for (i = 0; i < r->num_mirrors; i++) {
				mirror = &r->mirrors[i];
				if ((*a >= mirror->start) &&
					(*a <= mirror->end)) {
					*a = (*a - mirror->start) %
						(area->end - area->start + 1);
					region = r;
					break;
				}
			}
		}

		/* Break if region has been found */
		if (region)
			break;
	}

	/* Region has not been found */
	return region;
}

void memory_region_add(struct region *region)
{
	list_insert(&machine->regions, region);
}

void memory_region_remove_all()
{
	list_remove_all(&machine->regions);
}

uint8_t memory_readb(uint16_t address)
{
	struct list_link *regions = machine->regions;
	struct region *region;
	uint16_t a = address;

	while ((region = memory_find_region(&regions, &a))) {
		if (region->mops->readb)
			return region->mops->readb(region->data, a);
		a = address;
	}
	return 0;
}

uint16_t memory_readw(uint16_t address)
{
	struct list_link *regions = machine->regions;
	struct region *region;
	uint16_t a = address;

	while ((region = memory_find_region(&regions, &a))) {
		if (region->mops->readw)
			return region->mops->readw(region->data, a);
		a = address;
	}
	return 0;
}

void memory_writeb(uint8_t b, uint16_t address)
{
	struct list_link *regions = machine->regions;
	struct region *region;
	uint16_t a = address;

	while ((region = memory_find_region(&regions, &a))) {
		if (region->mops->writeb)
			region->mops->writeb(region->data, b, a);
		a = address;
	}
}

void memory_writew(uint16_t w, uint16_t address)
{
	struct list_link *regions = machine->regions;
	struct region *region;
	uint16_t a = address;

	while ((region = memory_find_region(&regions, &a))) {
		if (region->mops->writew)
			region->mops->writew(region->data, w, a);
		a = address;
	}
}

void *memory_map_file(char *path, int offset, int size)
{
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
}

void memory_unmap_file(void *data, int size)
{
	intptr_t pa_data = (intptr_t)data & ~(sysconf(_SC_PAGE_SIZE) - 1);
	size += (intptr_t)data - pa_data;
	munmap((void *)pa_data, size);
}

