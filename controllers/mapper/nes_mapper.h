#ifndef _NES_MAPPER_H
#define _NES_MAPPER_H

#include <stdint.h>

#define PRG_ROM_SIZE(cart_header) \
	(16384 * cart_header->prg_rom_size)
#define CHR_ROM_SIZE(cart_header) \
	(8192 * cart_header->chr_rom_size)
#define PRG_ROM_OFFSET(cart_header) \
	sizeof(struct cart_header)
#define CHR_ROM_OFFSET(cart_header) \
	(sizeof(struct cart_header) + 16384 * cart_header->prg_rom_size)

struct cart_header {
	uint32_t ines_constant;
	uint8_t prg_rom_size;
	uint8_t chr_rom_size;
	uint8_t flags6;
	uint8_t flags7;
	uint8_t prg_ram_size;
	uint8_t flags9;
	uint8_t flags10;
	uint8_t unused[5];
};

#endif

