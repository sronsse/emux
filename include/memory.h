#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdbool.h>
#include <stdint.h>
#include <list.h>
#include <log.h>
#include <resource.h>

#define KB(x) (x * 1024)
#define MB(x) (x * 1024 * 1024)

#define DECLARE_MEMORY_READ_OP(ext, type) \
	typedef type (*read##ext##_t)(region_data_t *, address_t);
#define DECLARE_MEMORY_WRITE_OP(ext, type) \
	typedef void (*write##ext##_t)(region_data_t *, type, address_t);

#define DECLARE_DMA_READ_OP(ext, type) \
	typedef type (*dma_read##ext##_t)(dma_channel_data_t *);
#define DECLARE_DMA_WRITE_OP(ext, type) \
	typedef void (*dma_write##ext##_t)(dma_channel_data_t *, type);

/* address_t size should match the maximum bus size of all supported machines */
typedef uint32_t address_t;

typedef void region_data_t;
typedef void dma_channel_data_t;

/* Declare memory read/write operation function pointers */
DECLARE_MEMORY_READ_OP(b, uint8_t)
DECLARE_MEMORY_READ_OP(w, uint16_t)
DECLARE_MEMORY_READ_OP(l, uint32_t)
DECLARE_MEMORY_WRITE_OP(b, uint8_t)
DECLARE_MEMORY_WRITE_OP(w, uint16_t)
DECLARE_MEMORY_WRITE_OP(l, uint32_t)

/* Declare DMA read/write operation function pointers */
DECLARE_DMA_READ_OP(b, uint8_t)
DECLARE_DMA_READ_OP(w, uint16_t)
DECLARE_DMA_READ_OP(l, uint32_t)
DECLARE_DMA_WRITE_OP(b, uint8_t)
DECLARE_DMA_WRITE_OP(w, uint16_t)
DECLARE_DMA_WRITE_OP(l, uint32_t)

struct mops {
	readb_t readb;
	readw_t readw;
	readl_t readl;
	writeb_t writeb;
	writew_t writew;
	writel_t writel;
};

struct region {
	struct resource *area;
	struct mops *mops;
	region_data_t *data;
};

struct bus {
	int id;
	int width;
	struct list_link *regions;
};

struct dma_ops {
	dma_readb_t readb;
	dma_readw_t readw;
	dma_readl_t readl;
	dma_writeb_t writeb;
	dma_writew_t writew;
	dma_writel_t writel;
};

struct dma_channel {
	struct resource *res;
	struct dma_ops *ops;
	dma_channel_data_t *data;
};

bool memory_bus_add(struct bus *bus);
void memory_bus_remove(struct bus *bus);
void memory_bus_remove_all();
bool memory_region_add(struct region *region);
void memory_region_remove(struct region *region);

void dma_channel_add(struct dma_channel *channel);
void dma_channel_remove(struct dma_channel *channel);
void dma_channel_remove_all();

extern struct list_link *busses;
extern struct list_link *dma_channels;
extern struct mops rom_mops;
extern struct mops ram_mops;

#define DEFINE_MEMORY_READ(ext, type) \
	static inline type memory_read##ext(int bus_id, address_t address) \
	{ \
		struct bus *bus; \
		struct region *r; \
		struct resource *mirror; \
		struct list_link *link; \
		address_t a; \
		int i; \
	\
		/* Find bus with matching ID */ \
		link = busses; \
		while ((bus = list_get_next(&link)) && (bus->id != bus_id)); \
	\
		/* Return 0 if none was found */ \
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
	static inline void memory_write##ext(int bus_id, type data, \
		address_t address) \
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
		/* Find bus with matching ID */ \
		link = busses; \
		while ((bus = list_get_next(&link)) && (bus->id != bus_id)); \
	\
		/* Return on failure */ \
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

#define DEFINE_DMA_READ(ext, type) \
	static inline type dma_read##ext(int channel) \
	{ \
		struct dma_channel *ch; \
		struct list_link *link = dma_channels; \
	\
		/* Find matching DMA channel and call read operation */ \
		while ((ch = list_get_next(&link))) \
			if ((ch->res->data.dma.channel == channel) && \
				ch->ops->read##ext) \
				return ch->ops->read##ext(ch->data); \
	\
		/* Return 0 in case of read failure */ \
		LOG_W("DMA channel not found (%s(%u))!\n", __func__, channel); \
		return 0; \
	}

#define DEFINE_DMA_WRITE(ext, type) \
	static inline void dma_write##ext(int channel, type data) \
	{ \
		struct dma_channel *ch; \
		struct list_link *link = dma_channels; \
	\
		/* Find matching DMA channel and call write operation */ \
		while ((ch = list_get_next(&link))) \
			if ((ch->res->data.dma.channel == channel) && \
				ch->ops->write##ext) { \
				ch->ops->write##ext(ch->data, data); \
				return; \
			} \
	\
		/* Warn in case of read failure */ \
		LOG_W("DMA channel not found (%s(%u))!\n", __func__, channel); \
	}

/* Define memory read/write functions */
DEFINE_MEMORY_READ(b, uint8_t)
DEFINE_MEMORY_READ(w, uint16_t)
DEFINE_MEMORY_READ(l, uint32_t)
DEFINE_MEMORY_WRITE(b, uint8_t)
DEFINE_MEMORY_WRITE(w, uint16_t)
DEFINE_MEMORY_WRITE(l, uint32_t)

/* Define DMA read/write functions */
DEFINE_DMA_READ(b, uint8_t)
DEFINE_DMA_READ(w, uint16_t)
DEFINE_DMA_READ(l, uint32_t)
DEFINE_DMA_WRITE(b, uint8_t)
DEFINE_DMA_WRITE(w, uint16_t)
DEFINE_DMA_WRITE(l, uint32_t)

#endif

