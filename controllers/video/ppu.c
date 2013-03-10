#include <stdbool.h>
#include <controller.h>
#include <util.h>

bool ppu_init(struct controller *UNUSED(controller))
{
	return true;
}

void ppu_deinit(struct controller *UNUSED(controller))
{
}

CONTROLLER_START(ppu)
	.init = ppu_init,
	.deinit = ppu_deinit
CONTROLLER_END

