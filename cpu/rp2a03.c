#include <stdbool.h>
#include <cpu.h>

bool rp2a03_init()
{
	return true;
}

void rp2a03_deinit()
{
}

CPU_START(rp2a03)
	.init = rp2a03_init,
	.deinit = rp2a03_deinit
CPU_END

