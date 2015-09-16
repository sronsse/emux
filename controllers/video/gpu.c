#include <controller.h>
#include <util.h>

static bool gpu_init(struct controller_instance *instance);
static void gpu_reset();
static void gpu_deinit(struct controller_instance *instance);

bool gpu_init(struct controller_instance *UNUSED(instance))
{
	return true;
}

void gpu_reset(struct controller_instance *UNUSED(instance))
{
}

void gpu_deinit(struct controller_instance *UNUSED(instance))
{
}

CONTROLLER_START(gpu)
	.init = gpu_init,
	.reset = gpu_reset,
	.deinit = gpu_deinit
CONTROLLER_END

