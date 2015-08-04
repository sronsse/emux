#include <controller.h>
#include <util.h>

static bool sms_ctrl_init(struct controller_instance *instance);
static void sms_ctrl_reset(struct controller_instance *instance);
static void sms_ctrl_deinit(struct controller_instance *instance);

bool sms_ctrl_init(struct controller_instance *UNUSED(instance))
{
	return true;
}

void sms_ctrl_reset(struct controller_instance *UNUSED(instance))
{
}

void sms_ctrl_deinit(struct controller_instance *UNUSED(instance))
{
}

CONTROLLER_START(sms_controller)
	.init = sms_ctrl_init,
	.reset = sms_ctrl_reset,
	.deinit = sms_ctrl_deinit
CONTROLLER_END

