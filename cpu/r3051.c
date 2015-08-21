#include <cpu.h>
#include <util.h>

static bool r3051_init(struct cpu_instance *instance);
static void r3051_reset(struct cpu_instance *instance);
static void r3051_deinit(struct cpu_instance *instance);

bool r3051_init(struct cpu_instance *UNUSED(instance))
{
	return true;
}

void r3051_reset(struct cpu_instance *UNUSED(instance))
{
}

void r3051_deinit(struct cpu_instance *UNUSED(instance))
{
}

CPU_START(r3051)
	.init = r3051_init,
	.reset = r3051_reset,
	.deinit = r3051_deinit
CPU_END

