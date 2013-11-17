#include <stdlib.h>
#include <controller.h>
#include <memory.h>
#include <controllers/mapper/gb_mapper.h>

#define BANK_START	0x4000
#define BANK_SIZE	KB(16)

static bool rom_init(struct controller_instance *instance);
static void rom_deinit(struct controller_instance *instance);

bool rom_init(struct controller_instance *instance)
{
	struct gb_mapper_mach_data *mach_data = instance->mach_data;
	struct resource *area;
	uint8_t *bank;

	/* Map second ROM bank */
	bank = memory_map_file(mach_data->cart_path, BANK_START, BANK_SIZE);

	/* Add second ROM bank */
	area = resource_get("rom1",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	memory_region_add(area, &rom_mops, bank);

	/* Save private data */
	instance->priv_data = bank;

	return true;
}

void rom_deinit(struct controller_instance *instance)
{
	uint8_t *bank = instance->priv_data;
	memory_unmap_file(bank, BANK_SIZE);
}

CONTROLLER_START(rom)
	.init = rom_init,
	.deinit = rom_deinit
CONTROLLER_END

