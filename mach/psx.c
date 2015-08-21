#include <machine.h>
#include <util.h>

static bool psx_init(struct machine *machine);
static void psx_deinit(struct machine *machine);

bool psx_init(struct machine *UNUSED(machine))
{
	return true;
}

void psx_deinit(struct machine *UNUSED(machine))
{
}

MACHINE_START(psx, "Sony PlayStation")
	.init = psx_init,
	.deinit = psx_deinit
MACHINE_END

