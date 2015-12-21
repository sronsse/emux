#include <controller.h>
#include <util.h>

static bool timer_init(struct controller_instance *instance);
static void timer_deinit(struct controller_instance *instance);

bool timer_init(struct controller_instance *UNUSED(instance))
{
	return true;
}

void timer_deinit(struct controller_instance *UNUSED(instance))
{
}

CONTROLLER_START(psx_timer)
	.init = timer_init,
	.deinit = timer_deinit
CONTROLLER_END

