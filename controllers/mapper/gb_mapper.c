#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <controller.h>
#include <memory.h>
#include <util.h>
#include <controllers/mapper/gb_mapper.h>

static bool gb_mapper_init(struct controller_instance *instance);
static void gb_mapper_deinit(struct controller_instance *instance);

static char *mbcs[] = {
	"rom"	/* ROM ONLY */
};

bool gb_mapper_init(struct controller_instance *instance)
{
	struct gb_mapper_mach_data *mach_data = instance->mach_data;
	struct controller_instance *mbc_instance;
	struct cart_header *cart_header;
	uint8_t number;

	/* Map cart header */
	cart_header = memory_map_file(mach_data->path, CART_HEADER_START,
		sizeof(struct cart_header));
	if (!cart_header) {
		fprintf(stderr, "Could not map header from \"%s\"!\n",
			mach_data->path);
		return false;
	}

	/* Print header info */
	fprintf(stdout, "Title: %.*s\n", TITLE_SIZE, cart_header->title);
	fprintf(stdout, "Manufacturer code: %.*s\n", MANUFACTURER_CODE_SIZE,
		cart_header->manufacturer_code);
	fprintf(stdout, "CGB flag: %u\n", cart_header->cgb_flag);
	fprintf(stdout, "New licensee code: %.*s\n", NEW_LICENSEE_CODE_SIZE,
		cart_header->new_licensee_code);
	fprintf(stdout, "SGB flag: %u\n", cart_header->sgb_flag);
	fprintf(stdout, "Cartridge type: %02x\n", cart_header->cartridge_type);
	fprintf(stdout, "ROM size: %02x\n", cart_header->rom_size);
	fprintf(stdout, "RAM size: %02x\n", cart_header->ram_size);
	fprintf(stdout, "Destination code: %02x\n", cart_header->dest_code);
	fprintf(stdout, "Old licensee code: %02x\n",
		cart_header->old_licensee_code);
	fprintf(stdout, "ROM version: %02x\n", cart_header->rom_version);
	fprintf(stdout, "Header checksum: %02x\n",
		cart_header->header_checksum);
	fprintf(stdout, "Global checksum: %04x\n",
		cart_header->global_checksum);

	/* Get cart type number */
	number = cart_header->cartridge_type;

	/* Unmap cart header */
	memory_unmap_file(cart_header, sizeof(struct cart_header));

	/* Check if cart type is supported */
	if ((number >= ARRAY_SIZE(mbcs)) || !mbcs[number]) {
		fprintf(stderr, "Cart type %u is not supported!\n", number);
		return false;
	}

	/* Cart type is supported, so add actual controller */
	fprintf(stdout, "Cart type %u (%s) detected.\n", number, mbcs[number]);
	mbc_instance = malloc(sizeof(struct controller_instance));
	mbc_instance->controller_name = mbcs[number];
	mbc_instance->num_resources = instance->num_resources;
	mbc_instance->resources = instance->resources;
	mbc_instance->mach_data = instance->mach_data;
	instance->priv_data = mbc_instance;
	controller_add(mbc_instance);

	return true;
}

void gb_mapper_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(gb_mapper)
	.init = gb_mapper_init,
	.deinit = gb_mapper_deinit
CONTROLLER_END

