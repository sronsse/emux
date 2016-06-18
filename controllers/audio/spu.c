#include <controller.h>
#include <util.h>

static bool spu_init(struct controller_instance *instance);
static void spu_deinit(struct controller_instance *instance);

bool spu_init(struct controller_instance *UNUSED(instance))
{
	return true;
}

void spu_deinit(struct controller_instance *UNUSED(instance))
{
}

CONTROLLER_START(spu)
	.init = spu_init,
	.deinit = spu_deinit
CONTROLLER_END

