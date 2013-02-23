#include <stdlib.h>
#include <machine.h>

bool nes_init()
{
	cpu_add("rp2a03");
	controller_add("ppu", NULL);
	return true;
}

void nes_deinit()
{
}

MACHINE_START(nes, "Nintendo Entertainment System")
	.init = nes_init,
	.deinit = nes_deinit
MACHINE_END

