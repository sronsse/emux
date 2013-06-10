#ifndef _NES_MAPPER_H
#define _NES_MAPPER_H

#include <stdint.h>
#include <resource.h>

struct nes_mapper_mach_data {
	char *path;
	struct resource *resources;
	int num_resources;
};

struct cart_header {
	uint32_t ines_constant;
	uint8_t prg_rom_size;
	uint8_t chr_rom_size;
	uint8_t flags6;
	uint8_t flags7;
	uint8_t prg_ram_size;
	uint8_t flags9;
	uint8_t flags10;
};

#endif

