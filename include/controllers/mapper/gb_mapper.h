#ifndef _GB_MAPPER_H
#define _GB_MAPPER_H

#include <stdint.h>

#define NINTENDO_LOGO_SIZE	48
#define MANUFACTURER_SIZE	4
#define NEW_LICENSEE_SIZE	2
#define TITLE_SIZE		11
#define ROM_BANK_SIZE		KB(16)
#define RAM_BANK_SIZE		KB(8)
#define CART_HEADER_START	0x0100
#define BOOTROM_SIZE		256

struct gb_mapper_mach_data {
	char *bootrom_path;
	char *cart_path;
};

struct cart_header {
	uint32_t entry_point;
	uint8_t nintendo_logo[NINTENDO_LOGO_SIZE];
	char title[TITLE_SIZE];
	char manufacturer[MANUFACTURER_SIZE];
	uint8_t cgb_flag;
	char new_licensee[NEW_LICENSEE_SIZE];
	uint8_t sgb_flag;
	uint8_t cartridge_type;
	uint8_t rom_size;
	uint8_t ram_size;
	uint8_t dest_code;
	uint8_t old_licensee_code;
	uint8_t rom_version;
	uint8_t header_checksum;
	uint16_t global_checksum;
};


int gb_mapper_get_rom_size(struct cart_header *cart_header);
int gb_mapper_get_ram_size(struct cart_header *cart_header);

#endif

