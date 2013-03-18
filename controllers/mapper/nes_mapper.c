#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <controller.h>
#include <memory.h>
#include <util.h>
#include <controllers/mapper/nes_mapper.h>

#define INES_CONSTANT 0x1A53454E

static bool nes_mapper_init(struct controller_instance *instance);
static void nes_mapper_deinit(struct controller_instance *instance);

static char *mappers[] = {
	"nrom"		/* No Mapper (or unknown mapper) */
};

bool nes_mapper_init(struct controller_instance *instance)
{
	struct nes_mapper_mach_data *mach_data = instance->mach_data;
	struct controller_instance *mapper_instance;
	struct cart_header *cart_header;
	uint8_t number;

	/* Map cart header */
	cart_header = memory_map_file(mach_data->path, 0,
		sizeof(struct cart_header));
	if (!cart_header) {
		fprintf(stderr, "Could not map header from \"%s\"!\n",
			mach_data->path);
		return false;
	}

	/* Validate header */
	if (cart_header->ines_constant != INES_CONSTANT) {
		fprintf(stderr, "Cart header does not have valid format!\n");
		memory_unmap_file(cart_header, sizeof(struct cart_header));
		return false;
	}

	/* Print header info */
	fprintf(stdout, "PRG ROM size: %u\n", cart_header->prg_rom_size);
	fprintf(stdout, "CHR ROM size: %u\n", cart_header->chr_rom_size);
	fprintf(stdout, "Flags 6: %02x\n", cart_header->flags6);
	fprintf(stdout, "Flags 7: %02x\n", cart_header->flags7);
	fprintf(stdout, "PRG RAM size: %u\n", cart_header->prg_ram_size);
	fprintf(stdout, "Flags 9: %02x\n", cart_header->flags9);
	fprintf(stdout, "Flags 10: %02x\n", cart_header->flags10);

	/* Bits 4-7 of flags 6 contain the mapper number lower nibble */
	/* Bits 4-7 of flags 7 contain the mapper number upper nibble */
	number = (cart_header->flags6 >> 4) | (cart_header->flags7 & 0xF0);

	/* Unmap cart header */
	memory_unmap_file(cart_header, sizeof(struct cart_header));

	/* Check if mapper is supported */
	if ((number >= ARRAY_SIZE(mappers)) || !mappers[number]) {
		fprintf(stderr, "Mapper %u is not supported!\n", number);
		return false;
	}

	/* Mapper type is supported, so add actual controller */
	fprintf(stdout, "Mapper %u (%s) detected.\n", number, mappers[number]);
	mapper_instance = malloc(sizeof(struct controller_instance));
	mapper_instance->controller_name = mappers[number];
	mapper_instance->num_resources = instance->num_resources;
	mapper_instance->resources = instance->resources;
	mapper_instance->mach_data = instance->mach_data;
	instance->priv_data = mapper_instance;
	controller_add(mapper_instance);

	return true;
}

void nes_mapper_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(nes_mapper)
	.init = nes_mapper_init,
	.deinit = nes_mapper_deinit
CONTROLLER_END

