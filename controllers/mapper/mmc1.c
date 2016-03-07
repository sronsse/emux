#include <stdlib.h>
#include <string.h>
#include <bitops.h>
#include <controller.h>
#include <cpu.h>
#include <env.h>
#include <file.h>
#include <memory.h>
#include <util.h>
#include "nes_mapper.h"

#define PRG_ROM_BANK_SIZE	KB(16)
#define CHR_ROM_BANK_SIZE	KB(4)
#define CHR_RAM_SIZE		KB(8)

#define SHIFT_REG_RESET_VALUE	0x00
#define NUM_SHIFT_STEPS		5
#define MIRRORING_ONE_LOWER	0
#define MIRRORING_ONE_UPPER	1
#define MIRRORING_VERTICAL	2
#define MIRRORING_HORIZONTAL	3

union load {
	uint8_t raw;
	struct {
		uint8_t data:1;
		uint8_t unused:6;
		uint8_t reset:1;
	};
};

union control {
	uint8_t raw:5;
	struct {
		uint8_t mirroring:2;
		uint8_t prg_rom_bank_mode:2;
		uint8_t chr_rom_bank_mode:1;
	};
};

union prg_bank {
	uint8_t raw:5;
	struct {
		uint8_t bank:4;
		uint8_t prg_ram_enable:1;
	};
};

struct mmc1 {
	union control control;
	uint8_t chr_bank_0:5;
	uint8_t chr_bank_1:5;
	union prg_bank prg_bank;
	uint8_t shift_reg:5;
	int shift_reg_step;
	int num_prg_rom_banks;
	uint8_t *vram;
	uint8_t *prg_ram;
	uint8_t *prg_rom;
	uint8_t *chr_ram;
	uint8_t *chr_rom;
	int prg_rom_size;
	int chr_rom_size;
	struct region prg_rom_region;
	struct region chr_region;
	struct region load_region;
	struct region vram_region;
	struct region sram_region;
};

static bool mmc1_init(struct controller_instance *instance);
static void mmc1_reset(struct controller_instance *instance);
static void mmc1_deinit(struct controller_instance *instance);
static void mirror_address(struct mmc1 *mmc1, address_t *address);
static void remap_prg_rom(struct mmc1 *mmc1, address_t *address);
static void remap_chr_rom(struct mmc1 *mmc1, address_t *address);
static uint8_t vram_readb(struct mmc1 *mmc1, address_t address);
static uint16_t vram_readw(struct mmc1 *mmc1, address_t address);
static void vram_writeb(struct mmc1 *mmc1, uint8_t b, address_t address);
static void vram_writew(struct mmc1 *mmc1, uint16_t w, address_t address);
static uint8_t prg_rom_readb(struct mmc1 *mmc1, address_t address);
static uint16_t prg_rom_readw(struct mmc1 *mmc1, address_t address);
static uint8_t chr_rom_readb(struct mmc1 *mmc1, address_t address);
static uint16_t chr_rom_readw(struct mmc1 *mmc1, address_t address);
static void load_writeb(struct mmc1 *mmc1, uint8_t b, address_t a);

static struct mops vram_mops = {
	.readb = (readb_t)vram_readb,
	.readw = (readw_t)vram_readw,
	.writeb = (writeb_t)vram_writeb,
	.writew = (writew_t)vram_writew
};

static struct mops prg_rom_mops = {
	.readb = (readb_t)prg_rom_readb,
	.readw = (readw_t)prg_rom_readw
};

static struct mops chr_rom_mops = {
	.readb = (readb_t)chr_rom_readb,
	.readw = (readw_t)chr_rom_readw
};

static struct mops load_mops = {
	.writeb = (writeb_t)load_writeb,
};

void mirror_address(struct mmc1 *mmc1, address_t *address)
{
	bool bit;

	/* Adapt address in function of selected mirroring:
	- 0: one-screen, lower bank
	- 1: one-screen, upper bank
	- 2: vertical
	- 3: horizontal */
	switch (mmc1->control.mirroring) {
	case 0:
	case 1:
		/* TODO: handle on-screen mirroring */
		break;
	case 2:
		/* Clear bit 11 of address */
		*address &= ~BIT(11);
		break;
	case 3:
		/* Set bit 10 of address to bit 11 and clear bit 11 */
		bit = *address & BIT(11);
		*address &= ~BIT(10);
		*address |= (bit << 10);
		*address &= ~(bit << 11);
		break;
	}
}

void remap_prg_rom(struct mmc1 *mmc1, address_t *address)
{
	int banks[2];
	int bank;

	/* Get PRG ROM bank numbers based on mode:
	- 0, 1: switch 32 KB at $8000, ignoring low bit of bank number
	- 2: fix first bank at $8000 and switch 16 KB bank at $C000
	- 3: fix last bank at $C000 and switch 16 KB bank at $8000 */
	switch (mmc1->control.prg_rom_bank_mode) {
	case 0:
	case 1:
		banks[0] = mmc1->prg_bank.bank & 0xFE;
		banks[1] = mmc1->prg_bank.bank | 0x01;
		break;
	case 2:
		banks[0] = 0;
		banks[1] = mmc1->prg_bank.bank;
		break;
	case 3:
	default:
		banks[0] = mmc1->prg_bank.bank;
		banks[1] = mmc1->num_prg_rom_banks - 1;
		break;
	}

	/* Select appropriate bank based on address */
	bank = banks[*address / PRG_ROM_BANK_SIZE];

	/* Adapt address */
	*address = (*address % PRG_ROM_BANK_SIZE) + (bank * PRG_ROM_BANK_SIZE);
}

void remap_chr_rom(struct mmc1 *mmc1, address_t *address)
{
	int banks[2];
	int bank;

	/* Get CHR ROM bank numbers based on mode:
	- 0: switch 8 KB at a time
	- 1: switch two separate 4 KB banks */
	switch (mmc1->control.chr_rom_bank_mode) {
	case 0:
		banks[0] = mmc1->chr_bank_0 & 0xFE;
		banks[1] = mmc1->chr_bank_0 | 0x01;
		break;
	case 1:
	default:
		banks[0] = mmc1->chr_bank_0;
		banks[1] = mmc1->chr_bank_1;
		break;
	}

	/* Select appropriate bank based on address */
	bank = banks[*address / CHR_ROM_BANK_SIZE];

	/* Adapt address */
	*address = (*address % CHR_ROM_BANK_SIZE) + (bank * CHR_ROM_BANK_SIZE);
}

uint8_t vram_readb(struct mmc1 *mmc1, address_t address)
{
	mirror_address(mmc1, &address);
	return ram_mops.readb(mmc1->vram, address);
}

uint16_t vram_readw(struct mmc1 *mmc1, address_t address)
{
	mirror_address(mmc1, &address);
	return ram_mops.readw(mmc1->vram, address);
}

void vram_writeb(struct mmc1 *mmc1, uint8_t b, address_t address)
{
	mirror_address(mmc1, &address);
	ram_mops.writeb(mmc1->vram, b, address);
}

void vram_writew(struct mmc1 *mmc1, uint16_t w, address_t address)
{
	mirror_address(mmc1, &address);
	ram_mops.writew(mmc1->vram, w, address);
}

uint8_t prg_rom_readb(struct mmc1 *mmc1, address_t address)
{
	remap_prg_rom(mmc1, &address);
	return rom_mops.readb(mmc1->prg_rom, address);
}

uint16_t prg_rom_readw(struct mmc1 *mmc1, address_t address)
{
	remap_prg_rom(mmc1, &address);
	return rom_mops.readw(mmc1->prg_rom, address);
}

uint8_t chr_rom_readb(struct mmc1 *mmc1, address_t address)
{
	remap_chr_rom(mmc1, &address);
	return rom_mops.readb(mmc1->chr_rom, address);
}

uint16_t chr_rom_readw(struct mmc1 *mmc1, address_t address)
{
	remap_chr_rom(mmc1, &address);
	return rom_mops.readw(mmc1->chr_rom, address);
}

void load_writeb(struct mmc1 *mmc1, uint8_t b, address_t address)
{
	union load load;
	uint8_t data;
	uint16_t a;
	int reg;

	/* Fill load */
	load.raw = b;

	/* Writing a value with bit 7 set ($80 through $FF) to any address in
	$8000-$FFFF clears the shift register to its initial state. */
	if (load.reset) {
		mmc1->shift_reg = SHIFT_REG_RESET_VALUE;
		mmc1->shift_reg_step = 0;
		return;
	}

	/* On the first four writes, the MMC1 shifts bit 0 into a shift
	register. */
	if (mmc1->shift_reg_step < NUM_SHIFT_STEPS - 1) {
		mmc1->shift_reg >>= 1;
		mmc1->shift_reg |= load.data << (NUM_SHIFT_STEPS - 1);
		mmc1->shift_reg_step++;
		return;
	}

	/* On the fifth write, the MMC1 copies bit 0 and the shift register
	contents into an internal register. */
	data = (mmc1->shift_reg >> 1) | (load.data << (NUM_SHIFT_STEPS - 1));

	/* Internal register is selected by bits 14 and 13 of the address. */
	a = address;
	reg = bitops_getw(&a, 13, 2);

	/* Write selected register as follows:
	- Control	$8000-$9FFF
	- CHR bank 0	$A000-$BFFF
	- CHR bank 1	$C000-$DFFF
	- PRG bank	$E000-$FFFF */
	switch (reg) {
	case 0:
		mmc1->control.raw = data;
		break;
	case 1:
		mmc1->chr_bank_0 = data;
		break;
	case 2:
		mmc1->chr_bank_1 = data;
		break;
	case 3:
	default:
		mmc1->prg_bank.raw = data;
		break;
	}

	/* The shift register is then cleared. */
	mmc1->shift_reg = SHIFT_REG_RESET_VALUE;
	mmc1->shift_reg_step = 0;
}

bool mmc1_init(struct controller_instance *instance)
{
	struct mmc1 *mmc1;
	struct cart_header *cart_header;
	struct resource *res;
	char *path;

	/* Allocate MMC1 structure */
	instance->priv_data = calloc(1, sizeof(struct mmc1));
	mmc1 = instance->priv_data;

	/* Get cart path */
	path = env_get_data_path();

	/* Map cart header */
	cart_header = file_map(PATH_DATA,
		path,
		0,
		sizeof(struct cart_header));

	/* Save number of PRG ROM banks */
	mmc1->num_prg_rom_banks = cart_header->prg_rom_size;

	/* Add PRG ROM region */
	res = resource_get("prg_rom",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	mmc1->prg_rom_region.area = res;
	mmc1->prg_rom_region.mops = &prg_rom_mops;
	mmc1->prg_rom_region.data = mmc1;
	memory_region_add(&mmc1->prg_rom_region);

	/* Allocate PRG RAM */
	mmc1->prg_ram = calloc(PRG_RAM_SIZE(cart_header), sizeof(uint8_t));

	/* Allocate CHR RAM if needed */
	if (cart_header->chr_rom_size == 0)
		mmc1->chr_ram = calloc(CHR_RAM_SIZE, sizeof(uint8_t));

	/* Add CHR ROM or CHR RAM region */
	res = resource_get("chr",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	mmc1->chr_region.area = res;
	if (cart_header->chr_rom_size != 0) {
		mmc1->chr_region.mops = &chr_rom_mops;
		mmc1->chr_region.data = mmc1;
	} else {
		mmc1->chr_region.mops = &ram_mops;
		mmc1->chr_region.data = mmc1->chr_ram;
	}
	memory_region_add(&mmc1->chr_region);

	/* Add VRAM region */
	res = resource_get("vram",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	mmc1->vram_region.area = res;
	mmc1->vram_region.mops = &vram_mops;
	mmc1->vram_region.data = mmc1;
	memory_region_add(&mmc1->vram_region);
	mmc1->vram = instance->mach_data;

	/* Add PRG RAM region */
	res = resource_get("sram",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	mmc1->sram_region.area = res;
	mmc1->sram_region.mops = &ram_mops;
	mmc1->sram_region.data = mmc1->prg_ram;
	memory_region_add(&mmc1->sram_region);

	/* Add load region (taking the entire PRG ROM address space) */
	res = resource_get("prg_rom",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	mmc1->load_region.area = res;
	mmc1->load_region.mops = &load_mops;
	mmc1->load_region.data = mmc1;
	memory_region_add(&mmc1->load_region);

	/* Map PRG ROM */
	mmc1->prg_rom_size = PRG_ROM_SIZE(cart_header);
	mmc1->prg_rom = file_map(PATH_DATA,
		path,
		PRG_ROM_OFFSET(cart_header),
		mmc1->prg_rom_size);

	/* Map CHR ROM */
	mmc1->chr_rom_size = CHR_ROM_SIZE(cart_header);
	mmc1->chr_rom = file_map(PATH_DATA,
		path,
		CHR_ROM_OFFSET(cart_header),
		mmc1->chr_rom_size);

	return true;
}

void mmc1_reset(struct controller_instance *instance)
{
	struct mmc1 *mmc1 = instance->priv_data;

	/* Reset mirroring (one-screen) */
	mmc1->control.mirroring = 0;

	/* Reset PRG mode (fix last bank at $C000, switch bank at $8000) */
	mmc1->control.prg_rom_bank_mode = 3;

	/* Reset CHR mode */
	mmc1->control.chr_rom_bank_mode = 0;

	/* Reset remaining internal data to 0 */
	mmc1->chr_bank_0 = 0;
	mmc1->chr_bank_1 = 0;
	mmc1->prg_bank.raw = 0;
	mmc1->shift_reg = SHIFT_REG_RESET_VALUE;
	mmc1->shift_reg_step = 0;
}

void mmc1_deinit(struct controller_instance *instance)
{
	struct mmc1 *mmc1 = instance->priv_data;
	file_unmap(mmc1->prg_rom, mmc1->prg_rom_size);
	if (!mmc1->chr_ram)
		file_unmap(mmc1->chr_rom, mmc1->chr_rom_size);
	free(mmc1->prg_ram);
	free(mmc1->chr_ram);
	free(mmc1);
}

CONTROLLER_START(mmc1)
	.init = mmc1_init,
	.reset = mmc1_reset,
	.deinit = mmc1_deinit
CONTROLLER_END

