#include <controller.h>
#include <controllers/mapper/nes_mapper.h>

void nes_mapper_init(machine_data_t *machine_data)
{
}

void nes_mapper_deinit()
{
}

CONTROLLER_START(nes_mapper)
	.init = nes_mapper_init,
	.deinit = nes_mapper_deinit
CONTROLLER_END

