#include <stdlib.h>
#include <controller.h>
#include <util.h>

static bool psx_dma_init(struct controller_instance *instance);
static void psx_dma_deinit(struct controller_instance *instance);

bool psx_dma_init(struct controller_instance *UNUSED(instance))
{
	return true;
}

void psx_dma_deinit(struct controller_instance *UNUSED(instance))
{
}

CONTROLLER_START(psx_dma)
	.init = psx_dma_init,
	.deinit = psx_dma_deinit
CONTROLLER_END

