#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <machine.h>
#include <memory.h>

static struct region *memory_find_region(struct region_link **regions,
	uint16_t *a);

extern struct machine *machine;

struct region *memory_find_region(struct region_link **regions, uint16_t *a)
{
	struct region *region = NULL;
	struct resource *area;
	struct resource *mirror;
	int i;

	while (*regions) {
		/* Check region area */
		area = (*regions)->region->area;
		if ((*a >= area->start) && (*a <= area->end)) {
			*a -= area->start;
			region = (*regions)->region;
		} else {
			/* Check region mirrors */
			for (i = 0; i < (*regions)->region->num_mirrors; i++) {
				mirror = &(*regions)->region->mirrors[i];
				if ((*a >= mirror->start) &&
					(*a <= mirror->end)) {
					*a = (*a - mirror->start) %
						(area->end - area->start + 1);
					region = (*regions)->region;
					break;
				}
			}
		}

		/* Skip to next region */
		*regions = (*regions)->next;

		/* Break if region has been found */
		if (region)
			break;
	}

	/* Region has not been found */
	return region;
}

void memory_region_add(struct region *region)
{
	struct region_link *link;
	struct region_link *tail;

	/* Create new link */
	link = malloc(sizeof(struct region_link));
	link->region = region;
	link->next = NULL;

	/* Set head if needed */
	if (!machine->regions) {
		machine->regions = link;
		return;
	}

	/* Find tail and add link */
	tail = machine->regions;
	while (tail->next)
		tail = tail->next;
	tail->next = link;
}

void memory_region_remove_all()
{
	struct region_link *link;
	while (machine->regions) {
		link = machine->regions;
		machine->regions = machine->regions->next;
		free(link);
	}
}

uint8_t memory_readb(uint16_t address)
{
	struct region_link *regions = machine->regions;
	struct region *region;
	uint16_t a = address;

	while (region = memory_find_region(&regions, &a)) {
		if (region->mops->readb)
			return region->mops->readb(region, a);
		a = address;
	}
	return 0;
}

uint16_t memory_readw(uint16_t address)
{
	struct region_link *regions = machine->regions;
	struct region *region;
	uint16_t a = address;

	while (region = memory_find_region(&regions, &a)) {
		if (region->mops->readw)
			return region->mops->readw(region, a);
		a = address;
	}
	return 0;
}

void memory_writeb(uint8_t b, uint16_t address)
{
	struct region_link *regions = machine->regions;
	struct region *region;
	uint16_t a = address;

	while (region = memory_find_region(&regions, &a)) {
		if (region->mops->writeb)
			region->mops->writeb(region, b, a);
		a = address;
	}
}

void memory_writew(uint16_t w, uint16_t address)
{
	struct region_link *regions = machine->regions;
	struct region *region;
	uint16_t a = address;

	while (region = memory_find_region(&regions, &a)) {
		if (region->mops->writew)
			region->mops->writew(region, w, a);
		a = address;
	}
}

void *memory_map_file(char *path, int offset, int size)
{
	int fd;
	struct stat sb;
	void *data;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return NULL;

	fstat(fd, &sb);
	if (!S_ISREG(sb.st_mode)) {
		close(fd);
		return NULL;
	}

	data = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, offset);
	if (data == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	close(fd);
	return data;
}

void memory_unmap_file(void *data, int size)
{
	munmap(data, size);
}

