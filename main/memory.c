#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <bitops.h>
#include <memory.h>
#include <util.h>

static uint8_t rom_readb(uint8_t *rom, address_t address);
static uint16_t rom_readw(uint8_t *rom, address_t address);
static uint32_t rom_readl(uint8_t *rom, address_t address);
static uint8_t ram_readb(uint8_t *ram, address_t address);
static uint16_t ram_readw(uint8_t *ram, address_t address);
static uint32_t ram_readl(uint8_t *ram, address_t address);
static void ram_writeb(uint8_t *ram, uint8_t b, address_t address);
static void ram_writew(uint8_t *ram, uint16_t w, address_t address);
static void ram_writel(uint8_t *ram, uint32_t l, address_t address);

struct region **regions;
int num_regions;
struct dma_channel **dma_channels;
int num_dma_channels;

struct mops rom_mops = {
	.readb = (readb_t)rom_readb,
	.readw = (readw_t)rom_readw,
	.readl = (readl_t)rom_readl
};

struct mops ram_mops = {
	.readb = (readb_t)ram_readb,
	.readw = (readw_t)ram_readw,
	.readl = (readl_t)ram_readl,
	.writeb = (writeb_t)ram_writeb,
	.writew = (writew_t)ram_writew,
	.writel = (writel_t)ram_writel
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

uint32_t rom_readl(uint8_t *ram, address_t address)
{
	uint8_t *mem = ram + address;
	return (*(mem + 3) << 24) |
		(*(mem + 2) << 16) |
		(*(mem + 1) << 8) |
		*mem;
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

uint32_t ram_readl(uint8_t *ram, address_t address)
{
	uint8_t *mem = ram + address;
	return (*(mem + 3) << 24) |
		(*(mem + 2) << 16) |
		(*(mem + 1) << 8) |
		*mem;
}

void ram_writeb(uint8_t *ram, uint8_t b, address_t address)
{
	ram[address] = b;
}

void ram_writew(uint8_t *ram, uint16_t w, address_t address)
{
	uint8_t *mem = ram + address;
	*mem++ = (uint8_t)w;
	*mem = w >> 8;
}

void ram_writel(uint8_t *ram, uint32_t l, address_t address)
{
	uint8_t *mem = ram + address;
	*mem++ = (uint8_t)l;
	*mem++ = (uint8_t)(l >> 8);
	*mem++ = (uint8_t)(l >> 16);
	*mem = (uint8_t)(l >> 24);
}

void memory_region_add(struct region *region)
{
	/* Grow memory regions array */
	regions = realloc(regions, ++num_regions * sizeof(struct region *));

	/* Shift regions */
	memmove(&regions[1],
		regions,
		(num_regions - 1) * sizeof(struct region *));

	/* Insert region before others (it will take precedence on read ops) */
	regions[0] = region;
}

void memory_region_remove(struct region *region)
{
	int i;

	/* Remove last region if needed */
	if ((num_regions > 0) && (region == regions[num_regions - 1])) {
		regions = realloc(regions,
			--num_regions * sizeof(struct region *));
		return;
	}

	/* Find and remove region */
	for (i = 0; i < num_regions - 1; i++)
		if (regions[i] == region) {
			memmove(&regions[i],
				&regions[i + 1],
				(num_regions - i) * sizeof(struct region *));
			regions = realloc(regions,
				--num_regions * sizeof(struct region *));
		}
}

void memory_region_remove_all()
{
	/* Free all regions */
	free(regions);
	regions = NULL;
	num_regions = 0;
}

void dma_channel_add(struct dma_channel *channel)
{
	/* Grow DMA channels array */
	dma_channels = realloc(dma_channels,
		++num_dma_channels * sizeof(struct dma_channel *));

	/* Shift channels */
	memmove(&dma_channels[1],
		dma_channels,
		(num_dma_channels - 1) * sizeof(struct dma_channel *));

	/* Insert channel before others (it will take precedence on read ops) */
	dma_channels[0] = channel;
}

void dma_channel_remove(struct dma_channel *channel)
{
	int i;

	/* Remove last channel if needed */
	if ((num_dma_channels > 0) && (dma_channels[num_dma_channels - 1])) {
		dma_channels = realloc(dma_channels,
			--num_dma_channels * sizeof(struct dma_channel *));
		return;
	}

	/* Find and remove channel */
	for (i = 0; i < num_dma_channels - 1; i++)
		if (dma_channels[i] == channel) {
			memmove(&dma_channels[i],
				dma_channels[i + 1],
				(num_dma_channels - i) *
					sizeof(struct dma_channel *));
			dma_channels = realloc(dma_channels,
				--num_dma_channels *
					sizeof(struct dma_channel *));
		}
}

void dma_channel_remove_all()
{
	/* Free all channels */
	free(dma_channels);
	dma_channels = NULL;
	num_dma_channels = 0;
}

