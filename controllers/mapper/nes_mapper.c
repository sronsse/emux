#include <stdbool.h>
#include <stdio.h>
#include <controller.h>
#include <memory.h>
#include <util.h>
#include <controllers/mapper/nes_mapper.h>

#define INES_CONSTANT 0x1A53454E

static char *mappers[] = {
	"nrom"		/* No Mapper (or unknown mapper) */
};

bool nes_mapper_init(struct controller *controller)
{
	struct nes_mapper_mach_data *mdata = controller->mdata;
	struct cart_header *cart_header;
	uint8_t number;

	/* Map cart header */
	cart_header = memory_map_file(mdata->path, 0,
		sizeof(struct cart_header));
	if (!cart_header) {
		fprintf(stderr, "Could not map header from \"%s\"!\n",
			mdata->path);
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
	controller_add(mappers[number], controller->mdata);

	return true;
}

CONTROLLER_START(nes_mapper)
	.init = nes_mapper_init
CONTROLLER_END

