#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <clock.h>
#include <controller.h>
#include <memory.h>
#include <resource.h>
#include <util.h>

#define N_REGS				8
#define CONTROL_REG_1_ADDR		0
#define CONTROL_REG_2_ADDR		1
#define STATUS_REG_ADDR			2
#define SPR_RAM_ADDR_REG_ADDR		3
#define SPR_RAM_DATA_REG_ADDR		4
#define BG_SCROLLING_OFFSET_ADDR	5
#define VRAM_ADDR_REG_ADDR		6
#define VRAM_READ_WRITE_DATA_REG_ADDR	7
#define VRAM_SIZE			0x4000

#define VRAM_ADDR_MSB_MASK		0x3F
#define N_DOTS_PER_SCANLINE		341
#define N_SCANLINES			262
#define VBLANK_PERIOD			20

static bool ppu_init(struct controller_instance *instance);
static void ppu_deinit(struct controller_instance *instance);
static void ppu_tick(clock_data_t *data);
static uint8_t ppu_readb(region_data_t *data, uint16_t address);
static void ppu_writeb(region_data_t *data, uint8_t b, uint16_t address);

struct ppu_control_reg_1 {
	uint8_t name_table_scroll_addr: 2;
	uint8_t port_2007_vram_addr_increment: 1;
	uint8_t pattern_table_addr_8x8_sprites: 1;
	uint8_t pattern_table_addr_background: 1;
	uint8_t sprite_size: 1;
	uint8_t ppu_master_slave_selection: 1;
	uint8_t execute_nmi_on_vblank: 1;
};

struct ppu_control_reg_2 {
	uint8_t monochrome_mode: 1;
	uint8_t background_clipping: 1;
	uint8_t sprite_cipping: 1;
	uint8_t background_visibility: 1;
	uint8_t sprite_visibility: 1;
	uint8_t color_emphasis: 3;
};

struct ppu_status_reg {
	uint8_t not_used: 5;
	uint8_t lost_sprites: 1;
	uint8_t sprite_0_hit: 1;
	uint8_t vblank_flag: 1;
};

struct ppu {
	union {
		uint8_t regs[N_REGS];
		struct {
			struct ppu_control_reg_1 control_reg_1;
			struct ppu_control_reg_2 control_reg_2;
			struct ppu_status_reg status_reg;
			uint8_t spr_ram_addr_reg;
			uint8_t spr_ram_data_reg;
			uint8_t background_scrolling_offset;
			uint8_t vram_addr_reg;
			uint8_t vram_read_write_data_reg;
		};
	};
	uint8_t vram[VRAM_SIZE];
	int current_dot;
	int current_scanline;
	bool first_frame;
	bool flipflop_first_write;
	uint8_t flipflop_value;
	uint8_t horizontal_scroll_origin;
	uint8_t vertical_scroll_origin;
	uint16_t vram_address;
	struct region region;
	struct clock clock;
};

static struct mops ppu_mops = {
	.readb = ppu_readb,
	.writeb = ppu_writeb
};

uint8_t ppu_readb(region_data_t *data, uint16_t address)
{
	struct ppu *ppu = data;
	uint8_t b = ppu->regs[address];

	/* During first frame after reset, port 2007 is read-protected */
	if (ppu->first_frame && (address == VRAM_READ_WRITE_DATA_REG_ADDR))
		return 0;

	/* Control register 1, control register 2, spr-ram address register,
	spr-ram data register, background scrolling offset, and vram address
	register are write-only */
	if ((address == CONTROL_REG_1_ADDR) ||
		(address == CONTROL_REG_2_ADDR) ||
		(address == SPR_RAM_ADDR_REG_ADDR) ||
		(address == SPR_RAM_DATA_REG_ADDR) ||
		(address == BG_SCROLLING_OFFSET_ADDR) ||
		(address == VRAM_ADDR_REG_ADDR))
		return 0;

	/* Status register reads reset ports 2005/2006 first write flipflop */
	if (address == STATUS_REG_ADDR) {
		ppu->flipflop_first_write = true;

		/* Reading status register during VBLANK clears the flag */
		if (ppu->status_reg.vblank_flag)
			ppu->status_reg.vblank_flag = 0;
	} else if (address == VRAM_READ_WRITE_DATA_REG_ADDR) {
		/* Read from VRAM increasing address pointer accordingly */
		b = ppu->vram[ppu->vram_address];
		ppu->vram_address +=
			ppu->control_reg_1.port_2007_vram_addr_increment ?
			32 : 1;
	}

	return b;
}

void ppu_writeb(region_data_t *data, uint8_t b, uint16_t address)
{
	struct ppu *ppu = data;

	/* Status register is write-protected */
	if (address == STATUS_REG_ADDR)
		return;

	/* During first frame after reset, ports 2000, 2001, 2005, and 2006
	are write-protected */
	if (ppu->first_frame && ((address == CONTROL_REG_1_ADDR) ||
		(address == CONTROL_REG_2_ADDR) ||
		(address == BG_SCROLLING_OFFSET_ADDR) ||
		(address == VRAM_ADDR_REG_ADDR)))
		return;

	if ((address == BG_SCROLLING_OFFSET_ADDR) ||
		(address == VRAM_ADDR_REG_ADDR)) {
		/* Handle 1st write */
		if (ppu->flipflop_first_write) {
			ppu->flipflop_first_write = false;
			ppu->flipflop_value = b;
			return;
		}

		/* Handle 2nd write */
		if (address == BG_SCROLLING_OFFSET_ADDR) {
			ppu->horizontal_scroll_origin = ppu->flipflop_value;
			ppu->vertical_scroll_origin = b;
		} else {
			ppu->vram_address = ((ppu->flipflop_value &
				VRAM_ADDR_MSB_MASK) << 8) | b;
		}
		ppu->flipflop_first_write = true;
		return;
	} else if (address == VRAM_READ_WRITE_DATA_REG_ADDR) {
		/* Write to stored VRAM address and increment pointer */
		ppu->vram[ppu->vram_address] = b;
		ppu->vram_address +=
			ppu->control_reg_1.port_2007_vram_addr_increment ?
			32 : 1;
		return;
	}

	ppu->regs[address] = b;
}

bool ppu_init(struct controller_instance *instance)
{
	struct ppu *ppu;
	struct region *region;
	struct resource *clk = resource_get("dot_clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);

	/* Allocate PPU structure */
	instance->priv_data = malloc(sizeof(struct ppu));
	ppu = instance->priv_data;

	/* Initialize registers and data */
	ppu->control_reg_1.execute_nmi_on_vblank = 0;
	ppu->status_reg.vblank_flag = 1;
	ppu->current_dot = 0;
	ppu->current_scanline = 0;
	ppu->first_frame = true;
	ppu->flipflop_first_write = true;

	/* Set up PPU memory region */
	region = &ppu->region;
	region->area = resource_get("ppu",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	region->mops = &ppu_mops;
	region->data = instance->priv_data;
	memory_region_add(region);

	/* Set up clock */
	ppu->clock.rate = clk->rate;
	ppu->clock.data = ppu;
	ppu->clock.tick = ppu_tick;
	clock_add(&ppu->clock);

	return true;
}

void ppu_tick(clock_data_t *data)
{
	struct ppu *ppu = data;

	/* Reset VBLANK flag if needed */
	if ((ppu->current_dot == 0) && (ppu->current_scanline == VBLANK_PERIOD))
		ppu->status_reg.vblank_flag = 0;

	/* Check if first frame is complete */
	if (ppu->first_frame && ppu->current_scanline == N_SCANLINES - 1)
		ppu->first_frame = false;

	/* Increment dot/scanline counters, resetting BLANK flag if needed */
	if (++ppu->current_dot == N_DOTS_PER_SCANLINE) {
		ppu->current_dot = 0;
		if (++ppu->current_scanline == N_SCANLINES) {
			ppu->current_scanline = 0;
			ppu->status_reg.vblank_flag = 1;
		}
	}
}

void ppu_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(ppu)
	.init = ppu_init,
	.deinit = ppu_deinit
CONTROLLER_END

