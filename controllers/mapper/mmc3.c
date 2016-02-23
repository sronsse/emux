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

#define BANK_SELECT_DATA_START		0x0000
#define BANK_SELECT_DATA_END		0x1FFF
#define MIRRORING_PROTECT_START		0x2000
#define MIRRORING_PROTECT_END		0x3FFF
#define IRQ_LATCH_RELOAD_START		0x4000
#define IRQ_LATCH_RELOAD_END		0x5FFF
#define IRQ_DISABLE_ENABLE_START	0x6000
#define IRQ_DISABLE_ENABLE_END		0x7FFF

#define NUM_BANK_REGISTERS		8
#define NUM_PRG_ROM_BANKS		4
#define NUM_CHR_ROM_BANKS		8
#define PRG_ROM_BANK_SIZE		KB(8)
#define CHR_ROM_BANK_SIZE		KB(1)

union bank_select {
	uint8_t raw;
	struct {
		uint8_t reg:3;
		uint8_t unused:3;
		uint8_t prg_rom_bank_mode:1;
		uint8_t chr_a12_inversion:1;
	};
};

union mirroring {
	uint8_t raw;
	struct {
		uint8_t nametable_mirroring:1;
		uint8_t unused:7;
	};
};

struct mmc3 {
	uint8_t regs[NUM_BANK_REGISTERS];
	union bank_select bank_sel;
	uint8_t scanline_counter;
	uint8_t scanline_counter_latch;
	bool scanline_counter_reload;
	bool a12_state;
	bool irq_enable;
	bool irq_active;
	bool horizontal_mirroring;
	int num_prg_rom_banks;
	uint8_t *vram;
	uint8_t *prg_ram;
	uint8_t *prg_rom;
	uint8_t *chr_rom;
	int prg_rom_size;
	int chr_rom_size;
	struct resource bank_sel_data_area;
	struct resource mirror_protect_area;
	struct resource irq_latch_reload_area;
	struct resource irq_dis_en_area;
	struct region prg_rom_region;
	struct region chr_rom_region;
	struct region bank_sel_data_region;
	struct region mirror_protect_region;
	struct region irq_latch_reload_region;
	struct region irq_dis_en_region;
	struct region vram_region;
	struct region sram_region;
	int irq;
};

static bool mmc3_init(struct controller_instance *instance);
static void mmc3_reset(struct controller_instance *instance);
static void mmc3_deinit(struct controller_instance *instance);
static void mirror_address(struct mmc3 *mmc3, address_t *address);
static void remap_prg_rom(struct mmc3 *mmc3, address_t *address);
static void remap_chr_rom(struct mmc3 *mmc3, address_t *address);
static void chr_rom_access(struct mmc3 *mmc3, address_t address);
static uint8_t vram_readb(struct mmc3 *mmc3, address_t address);
static uint16_t vram_readw(struct mmc3 *mmc3, address_t address);
static void vram_writeb(struct mmc3 *mmc3, uint8_t b, address_t address);
static void vram_writew(struct mmc3 *mmc3, uint16_t w, address_t address);
static uint8_t prg_rom_readb(struct mmc3 *mmc3, address_t address);
static uint16_t prg_rom_readw(struct mmc3 *mmc3, address_t address);
static uint8_t chr_rom_readb(struct mmc3 *mmc3, address_t address);
static uint16_t chr_rom_readw(struct mmc3 *mmc3, address_t address);
static void bank_sel_data_writeb(struct mmc3 *mmc3, uint8_t b, address_t a);
static void mirror_protect_writeb(struct mmc3 *mmc3, uint8_t b, address_t a);
static void irq_latch_reload_writeb(struct mmc3 *mmc3, uint8_t b, address_t a);
static void irq_dis_en_writeb(struct mmc3 *mmc3, uint8_t b, address_t a);

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

static struct mops bank_sel_data_mops = {
	.writeb = (writeb_t)bank_sel_data_writeb,
};

static struct mops mirror_protect_mops = {
	.writeb = (writeb_t)mirror_protect_writeb,
};

static struct mops irq_latch_reload_mops = {
	.writeb = (writeb_t)irq_latch_reload_writeb,
};

static struct mops irq_dis_en_mops = {
	.writeb = (writeb_t)irq_dis_en_writeb,
};

void mirror_address(struct mmc3 *mmc3, address_t *address)
{
	bool bit;

	/* The NES hardware lets the cart control some VRAM lines as follows:
	Vertical mirroring: $2000 equals $2800 and $2400 equals $2C00
	Horizontal mirroring: $2000 equals $2400 and $2800 equals $2C00 */

	/* Adapt address in function of selected mirroring */
	if (mmc3->horizontal_mirroring) {
		/* Set bit 10 of address to bit 11 and clear bit 11 */
		bit = *address & BIT(11);
		*address &= ~BIT(10);
		*address |= (bit << 10);
		*address &= ~(bit << 11);
	} else {
		/* Clear bit 11 of address */
		*address &= ~BIT(11);
	}
}

void remap_prg_rom(struct mmc3 *mmc3, address_t *address)
{
	int slot;
	int bank;
	bool mode;

	/* Get slot based on address */
	slot = *address / PRG_ROM_BANK_SIZE;

	/* Get PRG ROM bank number based on bank register:
	Mode		0	1
	$8000-$9FFF	R6	(-2)
	$A000-$BFFF	R7	R7
	$C000-$DFFF	(-2)	R6
	$E000-$FFFF	(-1)	(-1) */
	mode = mmc3->bank_sel.prg_rom_bank_mode;
	switch (slot) {
	case 0:
		bank = !mode ? mmc3->regs[6] : mmc3->num_prg_rom_banks - 2;
		break;
	case 1:
		bank = mmc3->regs[7];
		break;
	case 2:
		bank = !mode ? mmc3->num_prg_rom_banks - 2 : mmc3->regs[6];
		break;
	case 3:
	default:
		bank = mmc3->num_prg_rom_banks - 1;
		break;
	}

	/* Adapt address */
	*address = (*address % PRG_ROM_BANK_SIZE) + (bank * PRG_ROM_BANK_SIZE);
}

void remap_chr_rom(struct mmc3 *mmc3, address_t *address)
{
	int slot;
	int bank;
	bool a12_inversion;

	/* Get slot based on address */
	slot = *address / CHR_ROM_BANK_SIZE;

	/* Get CHR ROM bank number based on bank register:
	A12 inversion	0		1
	$0000-$03FF	R0 AND $FE	R2
	$0400-$07FF	R0 OR 1		R3
	$0800-$0BFF	R1 AND $FE	R4
	$0C00-$0FFF	R1 OR 1		R5
	$1000-$13FF	R2		R0 AND $FE
	$1400-$17FF	R3		R0 OR 1
	$1800-$1BFF	R4		R1 AND $FE
	$1C00-$1FFF	R5		R1 OR 1 */
	a12_inversion = mmc3->bank_sel.chr_a12_inversion;
	switch (slot) {
	case 0:
		bank = !a12_inversion ? mmc3->regs[0] & 0xFE : mmc3->regs[2];
		break;
	case 1:
		bank = !a12_inversion ? mmc3->regs[0] | 0x01 : mmc3->regs[3];
		break;
	case 2:
		bank = !a12_inversion ? mmc3->regs[1] & 0xFE : mmc3->regs[4];
		break;
	case 3:
		bank = !a12_inversion ? mmc3->regs[1] | 0x01 : mmc3->regs[5];
		break;
	case 4:
		bank = !a12_inversion ? mmc3->regs[2] : mmc3->regs[0] & 0xFE;
		break;
	case 5:
		bank = !a12_inversion ? mmc3->regs[3] : mmc3->regs[0] | 0x01;
		break;
	case 6:
		bank = !a12_inversion ? mmc3->regs[4] : mmc3->regs[1] & 0xFE;
		break;
	case 7:
	default:
		bank = !a12_inversion ? mmc3->regs[5] : mmc3->regs[1] | 0x01;
		break;
	}

	/* Adapt address */
	*address = (*address % CHR_ROM_BANK_SIZE) + (bank * CHR_ROM_BANK_SIZE);
}

void chr_rom_access(struct mmc3 *mmc3, address_t address)
{
	bool a12_state;
	bool reload;

	/* Check for A12 rising edge (used to detect new scanline) */
	a12_state = ((address & BIT(12)) != 0);
	if (a12_state && !mmc3->a12_state) {
		/* Check if scanline counter needs to be reloaded */
		reload = (mmc3->scanline_counter == 0);
		reload |= mmc3->scanline_counter_reload;
		if (reload) {
			/* Reload scanline counter and reset reload flag */
			mmc3->scanline_counter = mmc3->scanline_counter_latch;
			mmc3->scanline_counter_reload = false;
		} else {
			/* Decrement scanline counter */
			mmc3->scanline_counter--;
		}

		/* If the scanline counter is zero, an IRQ will be fired if IRQ
		generation is enabled. */
		if ((mmc3->scanline_counter == 0) && mmc3->irq_enable)
			cpu_interrupt(mmc3->irq);
	}
	/* Save A12 state */
	mmc3->a12_state = a12_state;
}

uint8_t vram_readb(struct mmc3 *mmc3, address_t address)
{
	mirror_address(mmc3, &address);
	return ram_mops.readb(mmc3->vram, address);
}

uint16_t vram_readw(struct mmc3 *mmc3, address_t address)
{
	mirror_address(mmc3, &address);
	return ram_mops.readw(mmc3->vram, address);
}

void vram_writeb(struct mmc3 *mmc3, uint8_t b, address_t address)
{
	mirror_address(mmc3, &address);
	ram_mops.writeb(mmc3->vram, b, address);
}

void vram_writew(struct mmc3 *mmc3, uint16_t w, address_t address)
{
	mirror_address(mmc3, &address);
	ram_mops.writew(mmc3->vram, w, address);
}

uint8_t prg_rom_readb(struct mmc3 *mmc3, address_t address)
{
	remap_prg_rom(mmc3, &address);
	return rom_mops.readb(mmc3->prg_rom, address);
}

uint16_t prg_rom_readw(struct mmc3 *mmc3, address_t address)
{
	remap_prg_rom(mmc3, &address);
	return rom_mops.readw(mmc3->prg_rom, address);
}

uint8_t chr_rom_readb(struct mmc3 *mmc3, address_t address)
{
	chr_rom_access(mmc3, address);
	remap_chr_rom(mmc3, &address);
	return rom_mops.readb(mmc3->chr_rom, address);
}

uint16_t chr_rom_readw(struct mmc3 *mmc3, address_t address)
{
	chr_rom_access(mmc3, address);
	remap_chr_rom(mmc3, &address);
	return rom_mops.readw(mmc3->chr_rom, address);
}

void bank_sel_data_writeb(struct mmc3 *mmc3, uint8_t b, address_t address)
{
	bool bank_select;

	/* Check register to update (even = bank select, odd = bank data) */
	bank_select = !(address & BIT(0));

	/* Handle appropriate register */
	if (bank_select) {
		/* Save bank select register */
		mmc3->bank_sel.raw = b;
	} else {
		/* Update bank number based on bank select register */
		mmc3->regs[mmc3->bank_sel.reg] = b;
	}
}

void mirror_protect_writeb(struct mmc3 *mmc3, uint8_t b, address_t address)
{
	bool mirror;
	union mirroring mirroring;

	/* Check register to update (even = mirroring, odd = PRG RAM protect) */
	mirror = !(address & BIT(0));

	/* Handle appropriate register */
	if (mirror) {
		/* Update nametable mirroring (0: vertical; 1: horizontal) */
		mirroring.raw = b;
		mmc3->horizontal_mirroring = mirroring.nametable_mirroring;
	} else {
		/* Though these bits are functional on the MMC3, their main
		purpose is to write-protect save RAM during power-off, so
		consider this a no-op. */
	}
}

void irq_latch_reload_writeb(struct mmc3 *mmc3, uint8_t b, address_t address)
{
	bool latch;

	/* Check register to update (even = IRQ latch, odd = IRQ reload) */
	latch = !(address & BIT(0));

	/* Handle appropriate register */
	if (latch) {
		/* This register specifies the IRQ counter reload value. When
		the IRQ counter is zero (or a reload is requested), this value
		will be copied to the IRQ counter at the next rising edge of the
		PPU address, presumably at PPU cycle 260 of the current
		scanline. */
		mmc3->scanline_counter_latch = b;
	} else {
		/* Writing any value to this register reloads the MMC3 IRQ
		counter at the NEXT rising edge of the PPU address, presumably
		at PPU cycle 260 of the current scanline. */
		mmc3->scanline_counter_reload = true;
	}
}

void irq_dis_en_writeb(struct mmc3 *mmc3, uint8_t UNUSED(b), address_t address)
{
	bool disable;

	/* Check register to update (even = IRQ disable, odd = IRQ enable) */
	disable = !(address & BIT(0));

	/* Handle appropriate register */
	if (disable) {
		/* Writing any value to this register will disable MMC3
		interrupts and acknowledge any pending interrupts. */
		mmc3->irq_enable = false;
		mmc3->irq_active = false;
	} else {
		/* Writing any value to this register will enable MMC3
		interrupts. */
		mmc3->irq_enable = true;
	}
}

bool mmc3_init(struct controller_instance *instance)
{
	struct mmc3 *mmc3;
	struct cart_header *cart_header;
	struct resource *res;
	char *path;
	address_t start;
	address_t end;
	address_t prg_rom_start;
	int prg_rom_bus_id;

	/* Allocate MMC3 structure */
	instance->priv_data = calloc(1, sizeof(struct mmc3));
	mmc3 = instance->priv_data;

	/* Get cart path */
	path = env_get_data_path();

	/* Map cart header */
	cart_header = file_map(PATH_DATA,
		path,
		0,
		sizeof(struct cart_header));

	/* Save number of PRG ROM banks (the PRG ROM banks are 8192 bytes in
	size, half the size of an iNES PRG ROM bank) */
	mmc3->num_prg_rom_banks = cart_header->prg_rom_size * 2;

	/* Add PRG ROM region */
	res = resource_get("prg_rom",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	mmc3->prg_rom_region.area = res;
	mmc3->prg_rom_region.mops = &prg_rom_mops;
	mmc3->prg_rom_region.data = mmc3;
	memory_region_add(&mmc3->prg_rom_region);

	/* Save PRG ROM area start and bus ID */
	prg_rom_start = res->data.mem.start;
	prg_rom_bus_id = res->data.mem.bus_id;

	/* Allocate PRG RAM */
	mmc3->prg_ram = calloc(PRG_RAM_SIZE(cart_header), sizeof(uint8_t));

	/* Add CHR ROM region */
	res = resource_get("chr",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	mmc3->chr_rom_region.area = res;
	mmc3->chr_rom_region.mops = &chr_rom_mops;
	mmc3->chr_rom_region.data = mmc3;
	memory_region_add(&mmc3->chr_rom_region);

	/* Add VRAM region */
	res = resource_get("vram",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	mmc3->vram_region.area = res;
	mmc3->vram_region.mops = &vram_mops;
	mmc3->vram_region.data = mmc3;
	memory_region_add(&mmc3->vram_region);
	mmc3->vram = instance->mach_data;

	/* Add PRG RAM region */
	res = resource_get("sram",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	mmc3->sram_region.area = res;
	mmc3->sram_region.mops = &ram_mops;
	mmc3->sram_region.data = mmc3->prg_ram;
	memory_region_add(&mmc3->sram_region);

	/* Add bank select/bank data region */
	start = prg_rom_start + BANK_SELECT_DATA_START;
	end = prg_rom_start + BANK_SELECT_DATA_END;
	mmc3->bank_sel_data_area.type = RESOURCE_MEM;
	mmc3->bank_sel_data_area.data.mem.bus_id = prg_rom_bus_id;
	mmc3->bank_sel_data_area.data.mem.start = start;
	mmc3->bank_sel_data_area.data.mem.end = end;
	mmc3->bank_sel_data_area.num_children = 0;
	mmc3->bank_sel_data_area.children = NULL;
	mmc3->bank_sel_data_region.area = &mmc3->bank_sel_data_area;
	mmc3->bank_sel_data_region.mops = &bank_sel_data_mops;
	mmc3->bank_sel_data_region.data = mmc3;
	memory_region_add(&mmc3->bank_sel_data_region);

	/* Add mirroring/PRG RAM protect region */
	start = prg_rom_start + MIRRORING_PROTECT_START;
	end = prg_rom_start + MIRRORING_PROTECT_END;
	mmc3->mirror_protect_area.type = RESOURCE_MEM;
	mmc3->mirror_protect_area.data.mem.bus_id = prg_rom_bus_id;
	mmc3->mirror_protect_area.data.mem.start = start;
	mmc3->mirror_protect_area.data.mem.end = end;
	mmc3->mirror_protect_area.num_children = 0;
	mmc3->mirror_protect_area.children = NULL;
	mmc3->mirror_protect_region.area = &mmc3->mirror_protect_area;
	mmc3->mirror_protect_region.mops = &mirror_protect_mops;
	mmc3->mirror_protect_region.data = mmc3;
	memory_region_add(&mmc3->mirror_protect_region);

	/* Add IRQ latch/reload region */
	start = prg_rom_start + IRQ_LATCH_RELOAD_START;
	end = prg_rom_start + IRQ_LATCH_RELOAD_END;
	mmc3->irq_latch_reload_area.type = RESOURCE_MEM;
	mmc3->irq_latch_reload_area.data.mem.bus_id = prg_rom_bus_id;
	mmc3->irq_latch_reload_area.data.mem.start = start;
	mmc3->irq_latch_reload_area.data.mem.end = end;
	mmc3->irq_latch_reload_area.num_children = 0;
	mmc3->irq_latch_reload_area.children = NULL;
	mmc3->irq_latch_reload_region.area = &mmc3->irq_latch_reload_area;
	mmc3->irq_latch_reload_region.mops = &irq_latch_reload_mops;
	mmc3->irq_latch_reload_region.data = mmc3;
	memory_region_add(&mmc3->irq_latch_reload_region);

	/* Add IRQ disable/enable region */
	start = prg_rom_start + IRQ_DISABLE_ENABLE_START;
	end = prg_rom_start + IRQ_DISABLE_ENABLE_END;
	mmc3->irq_dis_en_area.type = RESOURCE_MEM;
	mmc3->irq_dis_en_area.data.mem.bus_id = prg_rom_bus_id;
	mmc3->irq_dis_en_area.data.mem.start = start;
	mmc3->irq_dis_en_area.data.mem.end = end;
	mmc3->irq_dis_en_area.num_children = 0;
	mmc3->irq_dis_en_area.children = NULL;
	mmc3->irq_dis_en_region.area = &mmc3->irq_dis_en_area;
	mmc3->irq_dis_en_region.mops = &irq_dis_en_mops;
	mmc3->irq_dis_en_region.data = mmc3;
	memory_region_add(&mmc3->irq_dis_en_region);

	/* Save IRQ number */
	res = resource_get("irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	mmc3->irq = res->data.irq;

	/* Map PRG ROM */
	mmc3->prg_rom_size = PRG_ROM_SIZE(cart_header);
	mmc3->prg_rom = file_map(PATH_DATA,
		path,
		PRG_ROM_OFFSET(cart_header),
		mmc3->prg_rom_size);

	/* Map CHR ROM */
	mmc3->chr_rom_size = CHR_ROM_SIZE(cart_header);
	mmc3->chr_rom = file_map(PATH_DATA,
		path,
		CHR_ROM_OFFSET(cart_header),
		mmc3->chr_rom_size);

	return true;
}

void mmc3_reset(struct controller_instance *instance)
{
	struct mmc3 *mmc3 = instance->priv_data;

	/* Reset internal data */
	memset(mmc3->regs, 0, NUM_BANK_REGISTERS * sizeof(uint8_t));
	mmc3->bank_sel.raw = 0;
	mmc3->scanline_counter = 0;
	mmc3->scanline_counter_latch = 0;
	mmc3->scanline_counter_reload = false;
	mmc3->a12_state = false;
	mmc3->irq_enable = false;
	mmc3->irq_active = false;
	mmc3->horizontal_mirroring = false;
}

void mmc3_deinit(struct controller_instance *instance)
{
	struct mmc3 *mmc3 = instance->priv_data;
	file_unmap(mmc3->prg_rom, mmc3->prg_rom_size);
	file_unmap(mmc3->chr_rom, mmc3->chr_rom_size);
	free(mmc3->prg_ram);
	free(mmc3);
}

CONTROLLER_START(mmc3)
	.init = mmc3_init,
	.reset = mmc3_reset,
	.deinit = mmc3_deinit
CONTROLLER_END

