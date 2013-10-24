#include <stdlib.h>
#include <controller.h>
#include <memory.h>
#include <controllers/mapper/gb_mapper.h>

#define ROM0_SIZE KB(16)
#define ROM1_SIZE KB(16)

struct rom {
	uint8_t *rom0;
	uint8_t *rom1;
};

static bool rom_init(struct controller_instance *instance);
static void rom_deinit(struct controller_instance *instance);

bool rom_init(struct controller_instance *instance)
{
	struct rom *rom;
	struct gb_mapper_mach_data *mach_data = instance->mach_data;
	struct resource *area;

	/* Allocate ROM structure */
	instance->priv_data = malloc(sizeof(struct rom));
	rom = instance->priv_data;

	/* Map ROM0 and ROM1 (regions are continuous) */
	rom->rom0 = memory_map_file(mach_data->path, 0, ROM0_SIZE);
	rom->rom1 = memory_map_file(mach_data->path, ROM0_SIZE, ROM1_SIZE);

	/* Add first ROM bank */
	area = resource_get("rom0",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	memory_region_add(area, &rom_mops, rom->rom0);

	/* Add second ROM bank */
	area = resource_get("rom1",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	memory_region_add(area, &rom_mops, rom->rom1);

	return true;
}

void rom_deinit(struct controller_instance *instance)
{
	struct rom *rom = instance->priv_data;
	memory_unmap_file(rom->rom0, ROM0_SIZE);
	memory_unmap_file(rom->rom1, ROM1_SIZE);
	free(rom);
}

CONTROLLER_START(rom)
	.init = rom_init,
	.deinit = rom_deinit
CONTROLLER_END

