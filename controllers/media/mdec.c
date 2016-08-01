#include <controller.h>
#include <util.h>

static bool mdec_init(struct controller_instance *instance);
static void mdec_deinit(struct controller_instance *instance);

bool mdec_init(struct controller_instance *UNUSED(instance))
{
	return true;
}

void mdec_deinit(struct controller_instance *UNUSED(instance))
{
}

CONTROLLER_START(mdec)
	.init = mdec_init,
	.deinit = mdec_deinit
CONTROLLER_END

