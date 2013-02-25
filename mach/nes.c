#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <machine.h>
#include <memory.h>
#include <controllers/mapper/nes_mapper.h>

#define INES_CONSTANT 0x1A53454E

struct cart_header {
	uint32_t ines_constant;
	uint8_t prg_rom_size;
	uint8_t chr_rom_size;
	uint8_t flags6;
	uint8_t flags7;
	uint8_t prg_ram_size;
	uint8_t flags9;
	uint8_t flags10;
};

static void nes_print_usage();
static bool nes_validate_cart();

static struct nes_mapper_mach_data nes_mapper_mach_data;

void nes_print_usage()
{
	fprintf(stderr, "Valid nes options:\n");
	fprintf(stderr, "  -c, --cart    Game cart path\n");
}

bool nes_validate_cart()
{
	char *cart_path;
	struct cart_header *cart_header;

	/* Get cart option */
	if (!cmdline_parse_string("cart", 'c', &cart_path)) {
		fprintf(stderr, "Please provide a cart option!\n");
		return false;
	}

	/* Map cart header */
	cart_header = memory_map_file(cart_path, 0, sizeof(struct cart_header));
	if (!cart_header) {
		fprintf(stderr, "Could not map cart header from \"%s\"!\n",
			cart_path);
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

	/* Copy useful header information into mapper machine data structure */
	nes_mapper_mach_data.path = cart_path;

	/* Unmap cart header */
	memory_unmap_file(cart_header, sizeof(struct cart_header));

	/* Cart is valid */
	return true;
}

bool nes_init()
{
	/* Validate cart */
	if (!nes_validate_cart()) {
		nes_print_usage();
		return false;
	}

	/* Add CPU and controllers */
	cpu_add("rp2a03");
	controller_add("ppu", NULL);
	controller_add("nes_mapper", &nes_mapper_mach_data);

	return true;
}

void nes_deinit()
{
}

MACHINE_START(nes, "Nintendo Entertainment System")
	.init = nes_init,
	.deinit = nes_deinit
MACHINE_END

