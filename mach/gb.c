#include <stdio.h>
#include <cmdline.h>
#include <machine.h>
#include <util.h>

static bool gb_init();
static void gb_deinit();
static void gb_print_usage();

void gb_print_usage()
{
	fprintf(stderr, "Valid gb options:\n");
	fprintf(stderr, "  --bootrom    GameBoy boot ROM path\n");
	fprintf(stderr, "  --cart       Game cart path\n");
}

bool gb_init(struct machine *UNUSED(machine))
{
	char *path;

	/* Get bootrom option */
	if (!cmdline_parse_string("bootrom", &path)) {
		fprintf(stderr, "Please provide a bootrom option!\n");
		gb_print_usage();
		return false;
	}

	/* Get cart option */
	if (!cmdline_parse_string("cart", &path)) {
		fprintf(stderr, "Please provide a cart option!\n");
		gb_print_usage();
		return false;
	}

	return true;
}

void gb_deinit(struct machine *UNUSED(machine))
{
}

MACHINE_START(gb, "Nintendo Game Boy")
	.init = gb_init,
	.deinit = gb_deinit
MACHINE_END

