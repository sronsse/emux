#include <controller.h>

void ppu_init(machine_data_t *machine_data)
{
}

void ppu_deinit()
{
}

CONTROLLER_START(ppu)
	.init = ppu_init,
	.deinit = ppu_deinit
CONTROLLER_END

