#include <controller.h>

void ppu_init(struct controller *controller)
{
}

void ppu_deinit(struct controller *controller)
{
}

CONTROLLER_START(ppu)
	.init = ppu_init,
	.deinit = ppu_deinit
CONTROLLER_END

