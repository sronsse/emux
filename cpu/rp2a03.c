#include <stdint.h>
#include <stdio.h>
#include <cpu.h>

void rp2a03_init()
{
}

void rp2a03_deinit()
{
}

CPU_START(rp2a03)
	.init = rp2a03_init,
	.deinit = rp2a03_deinit
CPU_END

