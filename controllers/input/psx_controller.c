#include <controller.h>
#include <util.h>

static bool psx_ctrl_init(struct controller_instance *instance);
static void psx_ctrl_deinit(struct controller_instance *instance);

bool psx_ctrl_init(struct controller_instance *UNUSED(instance))
{
	return true;
}

void psx_ctrl_deinit(struct controller_instance *UNUSED(instance))
{
}

CONTROLLER_START(psx_controller)
	.init = psx_ctrl_init,
	.deinit = psx_ctrl_deinit
CONTROLLER_END

