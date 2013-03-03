#include <stdbool.h>
#include <controller.h>

bool ppu_init(struct controller *controller)
{
	return true;
}

void ppu_deinit(struct controller *controller)
{
}

CONTROLLER_START(ppu)
	.init = ppu_init,
	.deinit = ppu_deinit
CONTROLLER_END

