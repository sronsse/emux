#include <stdlib.h>
#include <controller.h>
#include <file.h>
#include <memory.h>
#include <controllers/mapper/gb_mapper.h>

struct rom_data {
	uint8_t *bank;
	struct region region;
};

static bool rom_init(struct controller_instance *instance);
static void rom_deinit(struct controller_instance *instance);

bool rom_init(struct controller_instance *instance)
{
	struct rom_data *rom_data;
	struct gb_mapper_mach_data *mach_data = instance->mach_data;
	struct resource *area;

	/* Allocate ROM structure */
	instance->priv_data = calloc(1, sizeof(struct rom_data));
	rom_data = instance->priv_data;

	/* Map second ROM bank (skip ROM0) */
	rom_data->bank = file_map(PATH_DATA,
		mach_data->cart_path,
		ROM_BANK_SIZE,
		ROM_BANK_SIZE);

	/* Add ROM area */
	area = resource_get("rom1",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	rom_data->region.area = area;
	rom_data->region.mops = &rom_mops;
	rom_data->region.data = rom_data->bank;
	memory_region_add(&rom_data->region);

	return true;
}

void rom_deinit(struct controller_instance *instance)
{
	struct rom_data *rom_data = instance->priv_data;
	file_unmap(rom_data->bank, ROM_BANK_SIZE);
	free(rom_data);
}

CONTROLLER_START(rom)
	.init = rom_init,
	.deinit = rom_deinit
CONTROLLER_END

