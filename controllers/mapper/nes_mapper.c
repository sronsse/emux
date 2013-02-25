#include <stdbool.h>
#include <stdio.h>
#include <controller.h>
#include <memory.h>
#include <controllers/mapper/nes_mapper.h>

static char *mappers[] = {
	"nrom",		/* Mapper 0: NROM - No Mapper (or unknown mapper) */
	"mmc1",		/* Mapper 1: MMC1 - PRG/32K/16K, VROM/8K/4K, NT */
};

void nes_mapper_init(machine_data_t *machine_data)
{
	struct nes_mapper_mach_data *data = machine_data;

	/* Check if mapper is supported */
	if ((data->number >= ARRAY_SIZE(mappers)) || !mappers[data->number]) {
		fprintf(stderr, "Mapper %u is not supported!\n", data->number);
		return;
	}
	fprintf(stdout, "Mapper %u (%s) detected.\n", data->number,
		mappers[data->number]);
}

void nes_mapper_deinit()
{
}

CONTROLLER_START(nes_mapper)
	.init = nes_mapper_init,
	.deinit = nes_mapper_deinit
CONTROLLER_END

