#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <machine.h>
#include <memory.h>
#include <controllers/mapper/nes_mapper.h>

static void nes_print_usage();

static struct nes_mapper_mach_data nes_mapper_mach_data;

void nes_print_usage()
{
	fprintf(stderr, "Valid nes options:\n");
	fprintf(stderr, "  -c, --cart    Game cart path\n");
}

bool nes_init()
{
	/* Get cart option */
	if (!cmdline_parse_string("cart", 'c', &nes_mapper_mach_data.path)) {
		fprintf(stderr, "Please provide a cart option!\n");
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

