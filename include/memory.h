#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdbool.h>
#include <stdint.h>
#include <list.h>
#include <resource.h>

#define KB(x) (x * 1024)
#define MB(x) (x * 1024 * 1024)

#define DECLARE_MEMORY_READ(ext, type) \
	type memory_read##ext(int bus_id, address_t address);
#define DECLARE_MEMORY_WRITE(ext, type) \
	void memory_write##ext(int bus_id, type data, address_t address);

#define DECLARE_MEMORY_READ_OP(ext, type) \
	typedef type (*read##ext##_t)(region_data_t *, address_t);
#define DECLARE_MEMORY_WRITE_OP(ext, type) \
	typedef void (*write##ext##_t)(region_data_t *, type, address_t);

#define DECLARE_DMA_READ(ext, type) \
	type dma_read##ext(int channel);
#define DECLARE_DMA_WRITE(ext, type) \
	void dma_write##ext(int channel, type data);

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

/* Declare memory read/write functions */
DECLARE_MEMORY_READ(b, uint8_t)
DECLARE_MEMORY_READ(w, uint16_t)
DECLARE_MEMORY_READ(l, uint32_t)
DECLARE_MEMORY_WRITE(b, uint8_t)
DECLARE_MEMORY_WRITE(w, uint16_t)
DECLARE_MEMORY_WRITE(l, uint32_t)

void dma_channel_add(struct dma_channel *channel);
void dma_channel_remove(struct dma_channel *channel);
void dma_channel_remove_all();

/* Declare DMA read/write functions */
DECLARE_DMA_READ(b, uint8_t)
DECLARE_DMA_READ(w, uint16_t)
DECLARE_DMA_READ(l, uint32_t)
DECLARE_DMA_WRITE(b, uint8_t)
DECLARE_DMA_WRITE(w, uint16_t)
DECLARE_DMA_WRITE(l, uint32_t)

extern struct mops rom_mops;
extern struct mops ram_mops;

#endif

