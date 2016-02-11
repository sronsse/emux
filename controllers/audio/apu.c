#include <controller.h>
#include <util.h>

static bool apu_init(struct controller_instance *instance);
static void apu_reset(struct controller_instance *instance);
static void apu_deinit(struct controller_instance *instance);

bool apu_init(struct controller_instance *UNUSED(instance))
{
	return true;
}

void apu_reset(struct controller_instance *UNUSED(instance))
{
}

void apu_deinit(struct controller_instance *UNUSED(instance))
{
}

CONTROLLER_START(apu)
	.init = apu_init,
	.reset = apu_reset,
	.deinit = apu_deinit
CONTROLLER_END

