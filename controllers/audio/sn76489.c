#include <controller.h>
#include <util.h>

static bool sn76489_init(struct controller_instance *instance);
static void sn76489_reset(struct controller_instance *instance);
static void sn76489_deinit(struct controller_instance *instance);

bool sn76489_init(struct controller_instance *UNUSED(instance))
{
	return true;
}

void sn76489_reset(struct controller_instance *UNUSED(instance))
{
}

void sn76489_deinit(struct controller_instance *UNUSED(instance))
{
}

CONTROLLER_START(sn76489)
	.init = sn76489_init,
	.reset = sn76489_reset,
	.deinit = sn76489_deinit
CONTROLLER_END

