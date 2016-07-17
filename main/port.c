#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <log.h>
#include <port.h>

#define NUM_PORTS 256

static void insert_region(struct port_region *r, struct resource *a);
static void remove_region(struct port_region *r, struct resource *a);
static bool fixup_port(struct port_region *region, port_t *port);

static struct list_link *port_regions;
static struct list_link **read_map;
static struct list_link **write_map;

void insert_region(struct port_region *r, struct resource *a)
{
	int start = a->data.port.start;
	int end = a->data.port.end;
	int i;

	/* Insert region into maps */
	for (i = start; i <= end; i++) {
		if (r->pops->read)
			list_insert_before(&read_map[i], r);
		if (r->pops->write)
			list_insert_before(&write_map[i], r);
	}
}

void remove_region(struct port_region *r, struct resource *a)
{
	int start = a->data.port.start;
	int end = a->data.port.end;
	int i;

	/* Remove region from maps */
	for (i = start; i <= end; i++) {
		list_remove(&read_map[i], r);
		list_remove(&write_map[i], r);
	}
}

bool port_region_add(struct port_region *region)
{
	int i;

	/* Initialize maps if needed */
	if (!port_regions) {
		read_map = calloc(NUM_PORTS, sizeof(struct list_link *));
		write_map = calloc(NUM_PORTS, sizeof(struct list_link *));
	}

	/* Insert region */
	list_insert(&port_regions, region);

	/* Fill maps for region area and its mirrors */
	insert_region(region, region->area);
	for (i = 0; i < region->area->num_children; i++)
		insert_region(region, &region->area->children[i]);

	return true;
}

void port_region_remove(struct port_region *region)
{
	int i;

	/* Remove region from maps */
	remove_region(region, region->area);
	for (i = 0; i < region->area->num_children; i++)
		remove_region(region, &region->area->children[i]);

	/* Remove region from list */
	list_remove(&port_regions, region);
}

void port_region_remove_all()
{
	struct list_link *link = port_regions;
	struct port_region *region;
	int i;

	/* Remove all regions from maps */
	while ((region = list_get_next(&link))) {
		remove_region(region, region->area);
		for (i = 0; i < region->area->num_children; i++)
			remove_region(region, &region->area->children[i]);
	}

	/* Remove all regions */
	list_remove_all(&port_regions);

	/* Free maps */
	free(read_map);
	free(write_map);
}

bool fixup_port(struct port_region *region, port_t *port)
{
	port_t start;
	port_t end;
	int i;

	/* Check main area first */
	start = region->area->data.port.start;
	end = region->area->data.port.end;
	if ((*port >= start) && (*port <= end)) {
		*port -= start;
		return true;
	}

	/* Check mirrors next */
	for (i = 0; i < region->area->num_children; i++) {
		start = region->area->children[i].data.port.start;
		end = region->area->children[i].data.port.end;
		if ((*port >= start) && (*port <= end)) {
			*port -= start;
			*port %= PORT_SIZE(region->area);
			return true;
		}
	}

	/* Port could not be fixed up */
	return false;
}

uint8_t port_read(port_t port)
{
	struct list_link *link = read_map[port];
	struct port_region *region;

	/* Get region */
	region = list_get_next(&link);
	if (!region) {
		LOG_W("Port region not found (read %02x)!\n", port);
		return 0;
	}

	/* Adapt port */
	if (!fixup_port(region, &port)) {
		LOG_E("Port %02x fixup failed!\n", port);
		return 0;
	}

	/* Call port operation */
	return region->pops->read(region->data, port);
}

void port_write(uint8_t b, port_t port)
{
	struct list_link *link = write_map[port];
	struct port_region *region;

	/* Get region */
	region = list_get_next(&link);
	if (!region) {
		LOG_W("Port region not found (write %02x)!\n", port);
		return;
	}

	/* Adapt port */
	if (!fixup_port(region, &port)) {
		LOG_E("Port %04x fixup failed!\n", port);
		return;
	}

	/* Call port operation */
	region->pops->write(region->data, b, port);
}

