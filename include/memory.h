#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdbool.h>
#include <stdint.h>
#include <list.h>
#include <log.h>
#include <resource.h>

#define KB(x) (x * 1024)
#define MB(x) (x * 1024 * 1024)

#define MEM_SIZE(area) \
	(area->data.mem.end - area->data.mem.start + 1)

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

void memory_region_add(struct region *region);
void memory_region_remove(struct region *region);
void memory_region_remove_all();

void dma_channel_add(struct dma_channel *channel);
void dma_channel_remove(struct dma_channel *channel);
void dma_channel_remove_all();

extern struct region **regions;
extern int num_regions;
extern struct dma_channel **dma_channels;
extern int num_dma_channels;
extern struct mops rom_mops;
extern struct mops ram_mops;

#define DEFINE_MEMORY_READ(ext, type) \
	static inline type memory_read##ext(int bus_id, address_t address) \
	{ \
		struct region *r; \
		struct resource *mirror; \
		address_t size; \
		address_t a; \
		int i; \
		int j; \
	\
		/* Parse regions */ \
		for (i = 0; i < num_regions; i++) { \
			r = regions[i]; \
	\
			/* Skip if region if operation is not supported */ \
			if (!r->mops->read##ext) \
				continue; \
	\
			/* Call operation if address is within area */ \
			if ((bus_id == r->area->data.mem.bus_id) && \
				(address >= r->area->data.mem.start) && \
				(address <= r->area->data.mem.end)) { \
				a = address - r->area->data.mem.start; \
				return r->mops->read##ext(r->data, a); \
			} \
	\
			/* Get region size */ \
			size = MEM_SIZE(r->area); \
	\
			/* Call operation if address is within a mirror */ \
			for (j = 0; j < r->area->num_children; j++) { \
				mirror = &r->area->children[j]; \
				if ((bus_id == mirror->data.mem.bus_id) && \
					(address >= mirror->data.mem.start) && \
					(address <= mirror->data.mem.end)) { \
					a = address - mirror->data.mem.start; \
					a %= size; \
					return r->mops->read##ext(r->data, a); \
				} \
			} \
		} \
	\
		/* Return 0 in case of read failure */ \
		LOG_W("Region not found in %s(%u, 0x%08x)!\n", \
			__func__, \
			bus_id, \
			address); \
		return 0; \
	}

#define DEFINE_MEMORY_WRITE(ext, type) \
	static inline void memory_write##ext(int bus_id, type data, \
		address_t addr) \
	{ \
		struct region *r; \
		struct resource *mirror; \
		address_t size; \
		address_t a; \
		int num; \
		int i; \
		int j; \
	\
		/* Parse regions */ \
		num = 0; \
		for (i = 0; i < num_regions; i++) { \
			r = regions[i]; \
	\
			/* Skip if region if operation is not supported */ \
			if (!r->mops->write##ext) \
				continue; \
	\
			/* Adapt address and call write operation if needed */ \
			if ((bus_id == r->area->data.mem.bus_id) && \
				(addr >= r->area->data.mem.start) && \
				(addr <= r->area->data.mem.end)) { \
				a = addr - r->area->data.mem.start; \
				r->mops->write##ext(r->data, data, a); \
				num++; \
			} \
	\
			/* Get region size */ \
			size = MEM_SIZE(r->area); \
	\
			/* Parse mirrors */ \
			for (j = 0; j < r->area->num_children; j++) { \
				mirror = &r->area->children[j]; \
	\
				/* Skip if address is not within mirror */ \
				if ((bus_id != mirror->data.mem.bus_id) || \
					!((addr >= mirror->data.mem.start) && \
					(addr <= mirror->data.mem.end))) \
					continue; \
	\
				/* Adapt address and call write operation */ \
				a = (addr - mirror->data.mem.start) % size; \
				r->mops->write##ext(r->data, data, a); \
				num++; \
			} \
		} \
	\
		/* Warn on write failure */ \
		if (num == 0) \
			LOG_W("Region not found in %s(%u, 0x%08x, 0x%0*x)!\n", \
				__func__, \
				bus_id, \
				addr, \
				sizeof(type) * 2, \
				data); \
	}

#define DEFINE_DMA_READ(ext, type) \
	static inline type dma_read##ext(int channel) \
	{ \
		struct dma_channel *ch; \
		int i; \
	\
		/* Find matching DMA channel and call read operation */ \
		for (i = 0; i < num_dma_channels; i++) { \
			ch = dma_channels[i]; \
			if ((ch->res->data.dma.channel == channel) && \
				ch->ops->read##ext) \
				return ch->ops->read##ext(ch->data); \
		} \
	\
		/* Return 0 in case of read failure */ \
		LOG_W("DMA channel not found in %s(%u))!\n", \
			__func__, \
			channel); \
		return 0; \
	}

#define DEFINE_DMA_WRITE(ext, type) \
	static inline void dma_write##ext(int channel, type data) \
	{ \
		struct dma_channel *ch; \
		int i; \
	\
		/* Find matching DMA channel and call write operation */ \
		for (i = 0; i < num_dma_channels; i++) { \
			ch = dma_channels[i]; \
			if ((ch->res->data.dma.channel == channel) && \
				ch->ops->write##ext) { \
				ch->ops->write##ext(ch->data, data); \
				return; \
			} \
		} \
	\
		/* Warn in case of read failure */ \
		LOG_W("DMA channel not found in %s(%u, 0x%0*x)!\n", \
			__func__, \
			channel, \
			sizeof(type) * 2, \
			data); \
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

