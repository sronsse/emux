#include <controller.h>
#include <util.h>

static bool psx_cdrom_init(struct controller_instance *instance);
static void psx_cdrom_deinit(struct controller_instance *instance);

bool psx_cdrom_init(struct controller_instance *UNUSED(instance))
{
	return true;
}

void psx_cdrom_deinit(struct controller_instance *UNUSED(instance))
{
}

CONTROLLER_START(psx_cdrom)
	.init = psx_cdrom_init,
	.deinit = psx_cdrom_deinit
CONTROLLER_END

