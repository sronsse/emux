#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <clock.h>
#include <controller.h>
#include <memory.h>
#include <resource.h>
#include <util.h>

/* DMA controller hard-coded destination address */
#define DEST_ADDRESS 0x2004

static bool nes_sprite_init(struct controller_instance *instance);
static void nes_sprite_deinit(struct controller_instance *instance);
static void nes_sprite_writeb(region_data_t *data, uint8_t b, uint16_t address);

struct nes_sprite {
	int bus_id;
	struct region region;
};

static struct mops nes_sprite_mops = {
	.writeb = nes_sprite_writeb
};

void nes_sprite_writeb(region_data_t *data, uint8_t b, uint16_t UNUSED(address))
{
	struct nes_sprite *nes_sprite = data;
	uint16_t src_address;
	int i;

	/* Input byte represents upper byte of source address */
	src_address = b << 8;

	/* Transfer 256 bytes */
	for (i = 0; i < 256; i++) {
		b = memory_readb(nes_sprite->bus_id, src_address++);
		memory_writeb(nes_sprite->bus_id, b, DEST_ADDRESS);
	}

	/* The transfer takes 512 clock cycles and halts execution unit */
	clock_consume(512);
}

bool nes_sprite_init(struct controller_instance *instance)
{
	struct nes_sprite *nes_sprite;
	struct region *region;

	/* Allocate nes_sprite structure */
	instance->priv_data = malloc(sizeof(struct nes_sprite));
	nes_sprite = instance->priv_data;

	/* Set up nes_sprite memory region */
	region = &nes_sprite->region;
	region->area = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	region->mops = &nes_sprite_mops;
	region->data = instance->priv_data;
	memory_region_add(region);

	/* Save bus ID for later use */
	nes_sprite->bus_id = instance->bus_id;

	return true;
}

void nes_sprite_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(nes_sprite)
	.init = nes_sprite_init,
	.deinit = nes_sprite_deinit
CONTROLLER_END

