#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <bitops.h>
#include <clock.h>
#include <controller.h>
#include <cpu.h>
#include <memory.h>
#include <resource.h>
#include <video.h>

/* PPU registers */
#define NUM_REGS		8
#define PPUCTRL			0
#define PPUMASK			1
#define PPUSTATUS		2
#define OAMADDR			3
#define OAMDATA			4
#define PPUSCROLL		5
#define PPUADDR			6
#define PPUDATA			7

/* PPU constant parameters */
#define SCREEN_WIDTH		256
#define SCREEN_HEIGHT		240
#define SCREEN_REFRESH_RATE	60.0988
#define NUM_DOTS		341
#define NUM_SCANLINES		262
#define OAM_SIZE		256
#define SEC_OAM_SIZE		32
#define PALETTE_SIZE		32
#define TILE_WIDTH		8
#define TILE_HEIGHT		8
#define PATTERN_TABLE_0_START	0x0000
#define PATTERN_TABLE_1_START	0x1000
#define NAME_TABLE_START	0x2000
#define ATTRIBUTE_TABLE_START	0x23C0
#define BG_PALETTE_START	0x3F00
#define SPRITE_PALETTE_START	0x3F10
#define TILE_SIZE		16
#define NUM_PALETTE_ENTRIES	4
#define NUM_CHROMA_VALUES	16
#define NUM_LUMA_VALUES		4
#define NUM_SPRITES		64
#define NUM_SPRITES_PER_LINE	8

/* PPU events sorted by priority */
#define EVENT_OUTPUT		BIT(0)
#define EVENT_SHIFT_BG		BIT(1)
#define EVENT_SHIFT_SPR		BIT(2)
#define EVENT_RELOAD_BG		BIT(3)
#define EVENT_FETCH_NT		BIT(4)
#define EVENT_FETCH_AT		BIT(5)
#define EVENT_FETCH_LOW_BG	BIT(6)
#define EVENT_FETCH_HIGH_BG	BIT(7)
#define EVENT_VBLANK_SET	BIT(8)
#define EVENT_VBLANK_CLEAR	BIT(9)
#define EVENT_LOOPY_INC_HORI_V	BIT(10)
#define EVENT_LOOPY_INC_VERT_V	BIT(11)
#define EVENT_LOOPY_SET_HORI_V	BIT(12)
#define EVENT_LOOPY_SET_VERT_V	BIT(13)
#define EVENT_SEC_OAM_CLEAR	BIT(14)
#define EVENT_SPRITE_EVAL	BIT(15)
#define EVENT_FETCH_SPRITE	BIT(16)

union ppu_ctrl {
	uint8_t value;
	struct {
		uint8_t base_name_table_address:2;
		uint8_t vram_addr_increment:1;
		uint8_t sprite_pattern_table_addr_8x8:1;
		uint8_t bg_pattern_table_addr:1;
		uint8_t sprite_size:1;
		uint8_t ppu_master_slave_select:1;
		uint8_t generate_nmi_on_vblank:1;
	};
};

union ppu_mask {
	uint8_t value;
	struct {
		uint8_t greyscale:1;
		uint8_t bg_show_left_col:1;
		uint8_t sprite_show_left_col:1;
		uint8_t bg_visibility:1;
		uint8_t sprite_visibility:1;
		uint8_t color_emphasis:3;
	};
};

union ppu_status {
	uint8_t value;
	struct {
		uint8_t reserved:5;
		uint8_t sprite_overflow:1;
		uint8_t sprite_0_hit:1;
		uint8_t vblank_flag:1;
	};
};

union ppu_vram_address {
	uint16_t value:15;
	struct {
		uint16_t coarse_x_scroll:5;
		uint16_t coarse_y_scroll:5;
		uint16_t h_nametable:1;
		uint16_t v_nametable:1;
		uint16_t fine_y_scroll:3;
	};
};

union ppu_attribute_address {
	uint16_t value:12;
	struct {
		uint16_t high_coarse_x:3;
		uint16_t high_coarse_y:3;
		uint16_t attribute_offset:4;
		uint16_t h_nametable:1;
		uint16_t v_nametable:1;
	};
};

union ppu_attribute {
	uint8_t value;
	struct {
		uint8_t top_left:2;
		uint8_t top_right:2;
		uint8_t bottom_left:2;
		uint8_t bottom_right:2;
	};
};

union ppu_palette_entry {
	uint8_t value:6;
	struct {
		uint8_t chroma:4;
		uint8_t luma:2;
	};
};

union ppu_sprite_attributes {
	uint8_t value;
	struct {
		uint8_t palette:2;
		uint8_t unimplemented:3;
		uint8_t priority:1;
		uint8_t h_flip:1;
		uint8_t v_flip:1;
	};
};

union ppu_sprite {
	uint8_t values[4];
	struct {
		uint8_t y;
		uint8_t tile_number;
		union ppu_sprite_attributes attributes;
		uint8_t x;
	};
};

struct ppu_render_data {
	uint8_t nt;
	uint8_t at:2;
	uint8_t bg_low;
	uint8_t bg_high;
	uint16_t shift_bg_low;
	uint16_t shift_bg_high;
	uint8_t attr_latch:2;
	uint8_t shift_at_low;
	uint8_t shift_at_high;
	uint8_t shift_spr_low[NUM_SPRITES_PER_LINE];
	uint8_t shift_spr_high[NUM_SPRITES_PER_LINE];
	uint8_t spr_attr_latches[NUM_SPRITES_PER_LINE];
	uint8_t x_counters[NUM_SPRITES_PER_LINE];
};

struct ppu {
	union ppu_ctrl ctrl;
	union ppu_mask mask;
	union ppu_status status;
	uint16_t oam_addr;
	union ppu_vram_address vram_addr;
	union ppu_vram_address temp_vram_addr;
	uint8_t fine_x_scroll:3;
	bool write_toggle;
	uint8_t vram_buffer;
	bool odd_frame;
	int h;
	int v;
	int sprite_counter;
	bool spr_0_evaluated;
	bool spr_0_fetched;
	int *events[NUM_SCANLINES];
	int visible_line[NUM_DOTS];
	int vblank_line[NUM_DOTS];
	int pre_render_line[NUM_DOTS];
	int idle_line[NUM_DOTS];
	struct ppu_render_data render_data;
	struct clock clock;
	uint8_t oam[OAM_SIZE];
	uint8_t sec_oam[SEC_OAM_SIZE];
	uint8_t palette[PALETTE_SIZE];
	int bus_id;
	int irq;
	struct region region;
	struct region palette_region;
};

typedef void (*ppu_event_t)(struct ppu *ppu);

static bool ppu_init(struct controller_instance *instance);
static void ppu_reset(struct controller_instance *instance);
static void ppu_deinit(struct controller_instance *instance);
static void ppu_tick(struct ppu *ppu);
static void ppu_update_counters(struct ppu *ppu);
static void ppu_set_events(struct ppu *ppu);
static void ppu_build_pre_render_line(struct ppu *ppu);
static void ppu_build_visible_line(struct ppu *ppu);
static void ppu_build_vblank_line(struct ppu *ppu);
static uint8_t palette_readb(uint8_t *ram, address_t address);
static void palette_writeb(uint8_t *ram, uint8_t b, address_t address);
static uint8_t ppu_readb(struct ppu *ppu, address_t address);
static void ppu_writeb(struct ppu *ppu, uint8_t b, address_t address);
static void ppu_output(struct ppu *ppu);
static void ppu_shift_bg(struct ppu *ppu);
static void ppu_shift_spr(struct ppu *ppu);
static void ppu_reload_bg(struct ppu *ppu);
static void ppu_fetch_nt(struct ppu *ppu);
static void ppu_fetch_at(struct ppu *ppu);
static void ppu_fetch_low_bg(struct ppu *ppu);
static void ppu_fetch_high_bg(struct ppu *ppu);
static void ppu_vblank_set(struct ppu *ppu);
static void ppu_vblank_clear(struct ppu *ppu);
static void ppu_loopy_inc_hori_v(struct ppu *ppu);
static void ppu_loopy_inc_vert_v(struct ppu *ppu);
static void ppu_loopy_set_hori_v(struct ppu *ppu);
static void ppu_loopy_set_vert_v(struct ppu *ppu);
static void ppu_sec_oam_clear(struct ppu *ppu);
static void ppu_sprite_eval(struct ppu *ppu);
static void ppu_fetch_sprite(struct ppu *ppu);

static struct mops palette_mops = {
	.readb = (readb_t)palette_readb,
	.writeb = (writeb_t)palette_writeb
};

static struct mops ppu_mops = {
	.readb = (readb_t)ppu_readb,
	.writeb = (writeb_t)ppu_writeb
};

static ppu_event_t ppu_events[] = {
	ppu_output,
	ppu_shift_bg,
	ppu_shift_spr,
	ppu_reload_bg,
	ppu_fetch_nt,
	ppu_fetch_at,
	ppu_fetch_low_bg,
	ppu_fetch_high_bg,
	ppu_vblank_set,
	ppu_vblank_clear,
	ppu_loopy_inc_hori_v,
	ppu_loopy_inc_vert_v,
	ppu_loopy_set_hori_v,
	ppu_loopy_set_vert_v,
	ppu_sec_oam_clear,
	ppu_sprite_eval,
	ppu_fetch_sprite
};

static struct color ppu_palette[NUM_LUMA_VALUES][NUM_CHROMA_VALUES] = {
	{
		{ 0x80, 0x80, 0x80 }, { 0x00, 0x3D, 0xA6 },
		{ 0x00, 0x12, 0xB0 }, { 0x44, 0x00, 0x96 },
		{ 0xA1, 0x00, 0x5E }, { 0xC7, 0x00, 0x28 },
		{ 0xBA, 0x06, 0x00 }, { 0x8C, 0x17, 0x00 },
		{ 0x5C, 0x2F, 0x00 }, { 0x10, 0x45, 0x00 },
		{ 0x05, 0x4A, 0x00 }, { 0x00, 0x47, 0x2E },
		{ 0x00, 0x41, 0x66 }, { 0x00, 0x00, 0x00 },
		{ 0x05, 0x05, 0x05 }, { 0x05, 0x05, 0x05 }
	},
	{
		{ 0xC7, 0xC7, 0xC7 }, { 0x00, 0x77, 0xFF },
		{ 0x21, 0x55, 0xFF }, { 0x82, 0x37, 0xFA },
		{ 0xEB, 0x2F, 0xB5 }, { 0xFF, 0x29, 0x50 },
		{ 0xFF, 0x22, 0x00 }, { 0xD6, 0x32, 0x00 },
		{ 0xC4, 0x62, 0x00 }, { 0x35, 0x80, 0x00 },
		{ 0x05, 0x8F, 0x00 }, { 0x00, 0x8A, 0x55 },
		{ 0x00, 0x99, 0xCC }, { 0x21, 0x21, 0x21 },
		{ 0x09, 0x09, 0x09 }, { 0x09, 0x09, 0x09 }
	},
	{
		{ 0xFF, 0xFF, 0xFF }, { 0x0F, 0xD7, 0xFF },
		{ 0x69, 0xA2, 0xFF }, { 0xD4, 0x80, 0xFF },
		{ 0xFF, 0x45, 0xF3 }, { 0xFF, 0x61, 0x8B },
		{ 0xFF, 0x88, 0x33 }, { 0xFF, 0x9C, 0x12 },
		{ 0xFA, 0xBC, 0x20 }, { 0x9F, 0xE3, 0x0E },
		{ 0x2B, 0xF0, 0x35 }, { 0x0C, 0xF0, 0xA4 },
		{ 0x05, 0xFB, 0xFF }, { 0x3E, 0x3E, 0x3E },
		{ 0x0D, 0x0D, 0x0D }, { 0x00, 0x00, 0x00 }
	},
	{
		{ 0xFF, 0xFF, 0xFF }, { 0xA6, 0xFC, 0xFF },
		{ 0xB3, 0xEC, 0xFF }, { 0xDA, 0xAB, 0xEB },
		{ 0xFF, 0xA8, 0xF9 }, { 0xFF, 0xAB, 0xB3 },
		{ 0xFF, 0xD2, 0xB0 }, { 0xFF, 0xEF, 0xA6 },
		{ 0xFF, 0xF7, 0x9C }, { 0xD7, 0xE8, 0x95 },
		{ 0xA6, 0xED, 0xAF }, { 0xA2, 0xF2, 0xDA },
		{ 0x99, 0xFF, 0xFC }, { 0xDD, 0xDD, 0xDD },
		{ 0x11, 0x11, 0x11 }, { 0x11, 0x11, 0x11 }
	}
};

uint8_t palette_readb(uint8_t *ram, address_t address)
{
	/* Addresses 0x3F10, 0x3F14, 0x3F18, 0x3F1C are mirrors of
	0x3F00, 0x3F04, 0x3F08, 0x3F0C */
	switch (address) {
	case 0x10:
	case 0x14:
	case 0x18:
	case 0x1C:
		address -= 0x10;
		break;
	default:
		break;
	}

	/* Read palette entry */
	return ram[address];
}

void palette_writeb(uint8_t *ram, uint8_t b, address_t address)
{
	/* Addresses 0x3F10, 0x3F14, 0x3F18, 0x3F1C are mirrors of
	0x3F00, 0x3F04, 0x3F08, 0x3F0C */
	switch (address) {
	case 0x10:
	case 0x14:
	case 0x18:
	case 0x1C:
		address -= 0x10;
		break;
	default:
		break;
	}

	/* Read palette entry */
	ram[address] = b;
}

uint8_t ppu_readb(struct ppu *ppu, address_t address)
{
	uint8_t b;

	switch (address) {
	case PPUSTATUS:
		/* w: = 0 */
		ppu->write_toggle = false;

		/* Read register */
		return ppu->status.value;
	case OAMDATA:
		/* Read from OAM */
		return ppu->oam[ppu->oam_addr];
	case PPUDATA:
		/* Read from VRAM incrementing address accordingly */
		b = ppu->vram_buffer;
		ppu->vram_buffer = memory_readb(ppu->bus_id,
			ppu->vram_addr.value);
		ppu->vram_addr.value += ppu->ctrl.vram_addr_increment ? 32 : 1;
		if (ppu->vram_addr.value >= BG_PALETTE_START)
			b = ppu->vram_buffer;
		return b;
	default:
		return 0;
	}
}

void ppu_writeb(struct ppu *ppu, uint8_t b, address_t address)
{
	uint16_t t;

	switch (address) {
	case PPUCTRL:
		/* Write register */
		ppu->ctrl.value = b;

		/* t: ...BA.. ........ = d: ......BA */
		ppu->temp_vram_addr.h_nametable = bitops_getb(&b, 0, 1);
		ppu->temp_vram_addr.v_nametable = bitops_getb(&b, 1, 1);
		break;
	case PPUMASK:
		/* Write register */
		ppu->mask.value = b;
		break;
	case OAMADDR:
		/* Write register */
		ppu->oam_addr = b;
		break;
	case OAMDATA:
		/* Store byte to OAM and increment address */
		ppu->oam[ppu->oam_addr++] = b;
		break;
	case PPUSCROLL:
		if (!ppu->write_toggle) {
			/* t: ....... ...HGFED = d: HGFED... */
			ppu->temp_vram_addr.coarse_x_scroll =
				bitops_getb(&b, 3, 5);

			/* x: CBA = d: .....CBA */
			ppu->fine_x_scroll = bitops_getb(&b, 0, 3);

			/* w: = 1 */
			ppu->write_toggle = true;
		} else {
			/* t: CBA..HG FED..... = d: HGFEDCBA */
			ppu->temp_vram_addr.coarse_y_scroll =
				bitops_getb(&b, 3, 5);
			ppu->temp_vram_addr.fine_y_scroll =
				bitops_getb(&b, 0, 3);

			/* w: = 0 */
			ppu->write_toggle = false;
		}
		break;
	case PPUADDR:
		if (!ppu->write_toggle) {
			/* t: .FEDCBA ........ = d: ..FEDCBA */
			/* t: X...... ........ = 0 */
			t = ppu->temp_vram_addr.value;
			bitops_setw(&t, 8, 7, bitops_getb(&b, 0, 6));
			ppu->temp_vram_addr.value = t;

			/* w: = 1 */
			ppu->write_toggle = true;
		} else {
			/* t: ....... HGFEDCBA = d: HGFEDCBA */
			t = ppu->temp_vram_addr.value;
			bitops_setw(&t, 0, 8, b);
			ppu->temp_vram_addr.value = t;

			/* v: = t */
			ppu->vram_addr.value = ppu->temp_vram_addr.value;

			/* w: = 0 */
			ppu->write_toggle = false;
		}
		break;
	case PPUDATA:
		/* Write to VRAM incrementing address accordingly */
		memory_writeb(ppu->bus_id, b, ppu->vram_addr.value);
		ppu->vram_addr.value += ppu->ctrl.vram_addr_increment ? 32 : 1;
		break;
	default:
		break;
	}
}

void ppu_output(struct ppu *ppu)
{
	struct ppu_render_data *r = &ppu->render_data;
	union ppu_sprite_attributes attributes;
	union ppu_palette_entry entry;
	uint16_t address;
	bool bg_priority;
	bool clipped;
	uint8_t bg_color = 0;
	uint8_t sprite_color = 0;
	uint8_t bg_palette = 0;
	uint8_t color;
	uint8_t palette;
	uint8_t l;
	uint8_t h;
	uint8_t x;
	int i;

	/* Get current X coordinate from H counter (output starts at h = 2) */
	x = ppu->h - 2;

	/* Check if background clipping is enabled and must be discarded */
	clipped = !ppu->mask.bg_show_left_col && (x < TILE_WIDTH);

	/* Only get data if background rendering is enabled (and not clipped) */
	if (ppu->mask.bg_visibility && !clipped) {
		/* Get palette index */
		l = bitops_getb(&r->shift_at_low, 7 - ppu->fine_x_scroll, 1);
		h = bitops_getb(&r->shift_at_high, 7 - ppu->fine_x_scroll, 1);
		bg_palette = l | (h << 1);

		/* Get pattern color */
		l = bitops_getw(&r->shift_bg_low, 15 - ppu->fine_x_scroll, 1);
		h = bitops_getw(&r->shift_bg_high, 15 - ppu->fine_x_scroll, 1);
		bg_color = l | (h << 1);
	}

	/* Check if sprite clipping is enabled and must be discarded */
	clipped = !ppu->mask.sprite_show_left_col && (x < TILE_WIDTH);

	/* Find first sprite opaque pixel if sprite rendering is enabled */
	if (ppu->mask.sprite_visibility && !clipped)
		for (i = 0; i < NUM_SPRITES_PER_LINE; i++) {
			/* Skip non-active sprites */
			if (ppu->render_data.x_counters[i] > 0)
				continue;

			/* Get pattern color */
			l = bitops_getb(&r->shift_spr_low[i], 7, 1);
			h = bitops_getb(&r->shift_spr_high[i], 7, 1);
			color = l | (h << 1);

			/* Skip if pixel is transparent */
			if (color == 0)
				continue;

			/* Set sprite 0 hit flag if needed */
			if ((i == 0) && (bg_color != 0) && ppu->spr_0_fetched)
				ppu->status.sprite_0_hit = 1;

			/* Save sprite information and break */
			sprite_color = color;
			attributes.value = r->spr_attr_latches[i];
			break;
		}

	/* Handle priority (background or sprite) */
	bg_priority = true;
	if ((bg_color == 0) && (sprite_color != 0))
		bg_priority = false;
	if ((bg_color != 0) && (sprite_color != 0) && !attributes.priority)
		bg_priority = false;

	/* Set color and palette based on priority multiplexer decision */
	color = bg_priority ? bg_color : sprite_color;
	palette = bg_priority ? bg_palette : attributes.palette;

	/* Compute palette entry (color 0 always points to first palette) */
	address = bg_priority ? BG_PALETTE_START : SPRITE_PALETTE_START;
	if (color != 0)
		address += NUM_PALETTE_ENTRIES * palette + color;

	/* Set pixel based on palette entry */
	entry.value = memory_readb(ppu->bus_id, address);
	video_set_pixel(x, ppu->v, ppu_palette[entry.luma][entry.chroma]);
}

void ppu_shift_bg(struct ppu *ppu)
{
	struct ppu_render_data *r = &ppu->render_data;
	uint8_t b;

	/* Return already if BG rendering is not enabled */
	if (!ppu->mask.bg_visibility)
		return;

	/* Shift background and palette shift registers */
	r->shift_bg_low <<= 1;
	r->shift_bg_high <<= 1;
	r->shift_at_low <<= 1;
	r->shift_at_high <<= 1;

	/* Reload palette shift registers */
	b = r->attr_latch;
	r->shift_at_low |= bitops_getb(&b, 0, 1);
	r->shift_at_high |= bitops_getb(&b, 1, 1);
}

void ppu_shift_spr(struct ppu *ppu)
{
	struct ppu_render_data *r = &ppu->render_data;
	int i;

	/* Return already if rendering is not enabled */
	if (!ppu->mask.sprite_visibility)
		return;

	/* Shift sprite tile registers if needed */
	for (i = 0; i < NUM_SPRITES_PER_LINE; i++)
		if (r->x_counters[i] == 0) {
			r->shift_spr_low[i] <<= 1;
			r->shift_spr_high[i] <<= 1;
		}

	/* Decrement X counters if needed */
	for (i = 0; i < NUM_SPRITES_PER_LINE; i++)
		if (r->x_counters[i] > 0)
			r->x_counters[i]--;
}

void ppu_reload_bg(struct ppu *ppu)
{
	struct ppu_render_data *r = &ppu->render_data;

	/* Return already if BG rendering is not enabled */
	if (!ppu->mask.bg_visibility)
		return;

	/* Load bitmap data into shift registers (lower 8 bits) */
	bitops_setw(&r->shift_bg_low, 0, 8, r->bg_low);
	bitops_setw(&r->shift_bg_high, 0, 8, r->bg_high);

	/* Load attribute latch */
	r->attr_latch = r->at;
}

void ppu_fetch_nt(struct ppu *ppu)
{
	address_t address;
	struct ppu_render_data *r = &ppu->render_data;
	union ppu_vram_address nametable_addr;

	/* Return already if BG rendering is not enabled */
	if (!ppu->mask.bg_visibility)
		return;

	/* Compute NT address */
	nametable_addr = ppu->vram_addr;
	nametable_addr.fine_y_scroll = 0;
	address = NAME_TABLE_START | nametable_addr.value;

	/* Get NT byte */
	r->nt = memory_readb(ppu->bus_id, address);
}

void ppu_fetch_at(struct ppu *ppu)
{
	address_t address;
	struct ppu_render_data *r = &ppu->render_data;
	union ppu_attribute_address attr_addr;
	union ppu_attribute at;
	uint8_t b;
	bool right;
	bool bottom;

	/* Return already if BG rendering is not enabled */
	if (!ppu->mask.bg_visibility)
		return;

	/* Compute AT address */
	b = ppu->vram_addr.coarse_x_scroll;
	attr_addr.high_coarse_x = bitops_getb(&b, 2, 3);
	b = ppu->vram_addr.coarse_y_scroll;
	attr_addr.high_coarse_y = bitops_getb(&b, 2, 3);
	attr_addr.h_nametable = ppu->vram_addr.h_nametable;
	attr_addr.v_nametable = ppu->vram_addr.v_nametable;
	address = ATTRIBUTE_TABLE_START | attr_addr.value;

	/* Fetch AT byte */
	at.value = memory_readb(ppu->bus_id, address);

	/* Get palette attributes */
	right = (ppu->vram_addr.coarse_x_scroll & BIT(1));
	bottom = (ppu->vram_addr.coarse_y_scroll & BIT(1));
	r->at = right ?
		bottom ? at.bottom_right : at.top_right :
		bottom ? at.bottom_left : at.top_left;
}

void ppu_fetch_low_bg(struct ppu *ppu)
{
	address_t address;
	struct ppu_render_data *r = &ppu->render_data;

	/* Return already if BG rendering is not enabled */
	if (!ppu->mask.bg_visibility)
		return;

	/* Select appropriate pattern table and compute address */
	address = (ppu->ctrl.bg_pattern_table_addr == 0) ?
		PATTERN_TABLE_0_START : PATTERN_TABLE_1_START;
	address += r->nt * TILE_SIZE + ppu->vram_addr.fine_y_scroll;

	/* Read low BG tile byte */
	r->bg_low = memory_readb(ppu->bus_id, address);
}

void ppu_fetch_high_bg(struct ppu *ppu)
{
	address_t address;
	struct ppu_render_data *r = &ppu->render_data;

	/* Return already if rendering is not enabled */
	if (!ppu->mask.bg_visibility)
		return;

	/* Select appropriate pattern table and compute address */
	address = (ppu->ctrl.bg_pattern_table_addr == 0) ?
		PATTERN_TABLE_0_START : PATTERN_TABLE_1_START;
	address += r->nt * TILE_SIZE + ppu->vram_addr.fine_y_scroll + 8;

	/* Read high BG tile byte */
	r->bg_high = memory_readb(ppu->bus_id, address);
}

void ppu_vblank_set(struct ppu *ppu)
{
	/* Set VBLANK flag and interrupt CPU if needed */
	ppu->status.vblank_flag = 1;
	if (ppu->ctrl.generate_nmi_on_vblank)
		cpu_interrupt(ppu->irq);

	/* Update screen contents */
	video_unlock();
	video_update();
}

void ppu_vblank_clear(struct ppu *ppu)
{
	/* Clear flags */
	ppu->status.vblank_flag = 0;
	ppu->status.sprite_overflow = 0;
	ppu->status.sprite_0_hit = 0;
	video_lock();
}

void ppu_loopy_inc_hori_v(struct ppu *ppu)
{
	/* Return already if BG rendering is not enabled */
	if (!ppu->mask.bg_visibility)
		return;

	/* Increment coarse X */
	ppu->vram_addr.coarse_x_scroll++;

	/* Toggle horizontal nametable on overflow */
	if (ppu->vram_addr.coarse_x_scroll == 0)
		ppu->vram_addr.h_nametable ^= 1;
}

void ppu_loopy_inc_vert_v(struct ppu *ppu)
{
	/* Return already if BG rendering is not enabled */
	if (!ppu->mask.bg_visibility)
		return;

	/* Increment fine Y */
	ppu->vram_addr.fine_y_scroll++;

	/* Return already if fine Y is different than 0 */
	if (ppu->vram_addr.fine_y_scroll != 0)
		return;

	/* Increment coarse Y on overflow */
	ppu->vram_addr.coarse_y_scroll++;

	/* Toggle vertical nametable if frame is complete */
	if (ppu->vram_addr.coarse_y_scroll == SCREEN_HEIGHT / TILE_HEIGHT) {
		ppu->vram_addr.coarse_y_scroll = 0;
		ppu->vram_addr.v_nametable ^= 1;
	}
}

void ppu_loopy_set_hori_v(struct ppu *ppu)
{
	/* Return already if BG rendering is not enabled */
	if (!ppu->mask.bg_visibility)
		return;

	/* Copy horizontal position from t to v */
	ppu->vram_addr.coarse_x_scroll = ppu->temp_vram_addr.coarse_x_scroll;
	ppu->vram_addr.h_nametable = ppu->temp_vram_addr.h_nametable;
}

void ppu_loopy_set_vert_v(struct ppu *ppu)
{
	/* Return already if BG rendering is not enabled */
	if (!ppu->mask.bg_visibility)
		return;

	/* Copy vertical position from t to v */
	ppu->vram_addr.coarse_y_scroll = ppu->temp_vram_addr.coarse_y_scroll;
	ppu->vram_addr.v_nametable = ppu->temp_vram_addr.v_nametable;
	ppu->vram_addr.fine_y_scroll = ppu->temp_vram_addr.fine_y_scroll;
}

void ppu_sec_oam_clear(struct ppu *ppu)
{
	/* Return already if sprite rendering is not enabled */
	if (!ppu->mask.sprite_visibility)
		return;

	/* Initialize secondary OAM */
	memset(ppu->sec_oam, 0xFF, SEC_OAM_SIZE);
}

void ppu_sprite_eval(struct ppu *ppu)
{
	union ppu_sprite *sprites;
	uint16_t address = 0;
	int num_sprites_found = 0;
	int height;
	int y;
	int n;
	int m;

	/* Return already if sprite rendering is not enabled */
	if (!ppu->mask.sprite_visibility)
		return;

	/* Set sprites address from primary OAM */
	sprites = (union ppu_sprite *)ppu->oam;

	/* Get height based on sprite size */
	height = ppu->ctrl.sprite_size ? 2 * TILE_HEIGHT : TILE_HEIGHT;

	/* Reset sprite 0 evaluated flag */
	ppu->spr_0_evaluated = false;

	/* Evaluate sprites */
	for (n = 0; n < NUM_SPRITES; n++) {
		/* Copy sprite Y coordinate to secondary OAM */
		ppu->sec_oam[address] = sprites[n].y;

		/* Skip if sprite is not in range */
		y = sprites[n].y;
		if ((ppu->v < y) || (ppu->v >= y + height))
			continue;

		/* Copy sprite remaining bytes to secondary OAM */
		address++;
		ppu->sec_oam[address++] = sprites[n].values[1];
		ppu->sec_oam[address++] = sprites[n].values[2];
		ppu->sec_oam[address++] = sprites[n].values[3];

		/* Flag sprite 0 as evaluated if needed */
		if (n == 0)
			ppu->spr_0_evaluated = true;

		/* Stop evaluation if maximum number of sprites is reached */
		if (++num_sprites_found == NUM_SPRITES_PER_LINE)
			break;
	}

	/* Handle sprite overflow */
	m = 0;
	while (n++ < NUM_SPRITES) {
		/* Evaluate OAM[address] as a Y coordinate */
		y = sprites[n].values[m];

		/* Check if sprite is in range */
		if ((ppu->v >= y) && (ppu->v < y + height)) {
			/* Set sprite overflow flag */
			ppu->status.sprite_overflow = 1;

			/* Skip next 3 bytes */
			m += 3;
		}

		/* Increment m every time (hardware bug) and handle overflow */
		if (++m >= 4)
			m -= 4;
	}
}

void ppu_fetch_sprite(struct ppu *ppu)
{
	union ppu_sprite *sprite;
	uint16_t address;
	bool transparent;
	int index;
	uint8_t low;
	uint8_t high;
	uint8_t tile_number;
	uint8_t y;

	/* Return already if sprite rendering is not enabled */
	if (!ppu->mask.sprite_visibility)
		return;

	/* Select sprite */
	index = ppu->sprite_counter;
	sprite = &((union ppu_sprite *)ppu->sec_oam)[index];

	/* Copy attributes and X coordinate */
	ppu->render_data.spr_attr_latches[index] = sprite->attributes.value;
	ppu->render_data.x_counters[index] = sprite->x;

	/* Increment sprite counter and handle overflow */
	if (++ppu->sprite_counter == NUM_SPRITES_PER_LINE)
		ppu->sprite_counter = 0;

	/* Set sprite 0 fetched flag if needed */
	if (index == 0)
		ppu->spr_0_fetched = ppu->spr_0_evaluated;

	/* Dummy fetches are replaced by transparent data */
	transparent = (sprite->y == 0xFF);
	if (transparent) {
		ppu->render_data.shift_spr_low[index] = 0;
		ppu->render_data.shift_spr_high[index] = 0;
		return;
	}

	/* Get relative Y coordinate from top of the tile */
	y = ppu->v - sprite->y;

	/* Select pattern table based on sprite size */
	if (!ppu->ctrl.sprite_size) {
		/* 8x8 pattern table is based on PPUCTRL */
		address = (ppu->ctrl.sprite_pattern_table_addr_8x8 == 0) ?
			PATTERN_TABLE_0_START : PATTERN_TABLE_1_START;

		/* Set tile number */
		tile_number = sprite->tile_number;
	} else {
		/* 8x16 pattern table is based on bit 0 of tile number */
		address = !(sprite->tile_number & BIT(0)) ?
			PATTERN_TABLE_0_START : PATTERN_TABLE_1_START;

		/* Set tile number (based on Y/vertical flip) */
		tile_number = sprite->tile_number & ~BIT(0);
		if ((y >= TILE_HEIGHT) != sprite->attributes.v_flip)
			tile_number++;
	}

	/* Add tile offset to address */
	address += tile_number * TILE_SIZE;

	/* Add line offset to address based on vertical flip */
	address += !sprite->attributes.v_flip ?
		y % TILE_HEIGHT : (TILE_HEIGHT - (y % TILE_HEIGHT) - 1);

	/* Fetch tile data */
	low = memory_readb(ppu->bus_id, address);
	high = memory_readb(ppu->bus_id, address + 8);

	/* Reverse data on horizontal flip */
	if (sprite->attributes.h_flip) {
		low = bitops_reverse(low, 8);
		high = bitops_reverse(high, 8);
	}

	/* Set tile data into shift registers */
	ppu->render_data.shift_spr_low[index] = low;
	ppu->render_data.shift_spr_high[index] = high;
}

void ppu_build_pre_render_line(struct ppu *ppu)
{
	int cycle;

	/* Cycle 0 */
		/* This is an idle cycle. The value on the PPU address bus
		during this cycle appears to be the same CHR address that is
		later used to fetch the low background tile byte starting at dot
		5 (possibly calculated during the two unused NT fetches at the
		end of the previous scanline). */

	/* Cycles 1-256 */
		/* Add clear VBLANK/sprite 0/overflow during tick 1. */
		cycle = 1;
		ppu->pre_render_line[cycle] |= EVENT_VBLANK_CLEAR;

		/* The data for each tile is fetched during this phase. Each
		memory access takes 2 PPU cycles to complete, and 4 must be
		performed per tile:
			- Nametable byte
			- Attribute table byte
			- Tile bitmap low
			- Tile bitmap high (+ 8 bytes from tile bitmap low) */
		for (cycle = 1; cycle <= 256; cycle += 8) {
			ppu->pre_render_line[cycle] |= EVENT_FETCH_NT;
			ppu->pre_render_line[cycle + 2] |= EVENT_FETCH_AT;
			ppu->pre_render_line[cycle + 4] |= EVENT_FETCH_LOW_BG;
			ppu->pre_render_line[cycle + 6] |= EVENT_FETCH_HIGH_BG;
		}

		/* BG and sprite shift registers shift during ticks 2...257. */
		for (cycle = 2; cycle <= 257; cycle++) {
			ppu->pre_render_line[cycle] |= EVENT_SHIFT_BG;
			ppu->pre_render_line[cycle] |= EVENT_SHIFT_SPR;
		}

		/* Shifters are reloaded during ticks 9, 17, ..., 257. */
		for (cycle = 9; cycle <= 257; cycle += 8)
			ppu->pre_render_line[cycle] |= EVENT_RELOAD_BG;

		/* Add inc hori(v) updates during ticks 8, 16, ..., 256. */
		for (cycle = 8; cycle <= 256; cycle += 8)
			ppu->pre_render_line[cycle] |= EVENT_LOOPY_INC_HORI_V;

		/* Add inc vert(v) update during tick 256. */
		cycle = 256;
		ppu->pre_render_line[cycle] |= EVENT_LOOPY_INC_VERT_V;

		/* Add hori(v) = hori(t) during tick 257. */
		cycle = 257;
		ppu->pre_render_line[cycle] |= EVENT_LOOPY_SET_HORI_V;

	/* Cycles 257-320 */
		/* The tile data for the sprites on the next scanline are
		fetched here. Again, each memory access takes 2 PPU cycles to
		complete, and 4 are performed for each of the 8 sprites:
		- Garbage nametable byte
		- Garbage nametable byte
		- Tile bitmap low
		- Tile bitmap high (+ 8 bytes from tile bitmap low) */
		for (cycle = 257; cycle < 320; cycle += 8)
			ppu->pre_render_line[cycle] |= EVENT_FETCH_SPRITE;

		/* Add vert(v) = vert(t) updates during ticks 280...304. */
		for (cycle = 280; cycle <= 304; cycle++)
			ppu->pre_render_line[cycle] |= EVENT_LOOPY_SET_VERT_V;

	/* Cycles 321-336 */
		/* This is where the first two tiles for the next scanline are
		fetched, and loaded into the shift registers. Again, each memory
		access takes 2 PPU cycles to complete, and 4 are performed for
		the two tiles:
			- Nametable byte
			- Attribute table byte
			- Tile bitmap low
			- Tile bitmap high (+ 8 bytes from tile bitmap low) */
		for (cycle = 321; cycle <= 336; cycle += 8) {
			ppu->pre_render_line[cycle] |= EVENT_FETCH_NT;
			ppu->pre_render_line[cycle + 2] |= EVENT_FETCH_AT;
			ppu->pre_render_line[cycle + 4] |= EVENT_FETCH_LOW_BG;
			ppu->pre_render_line[cycle + 6] |= EVENT_FETCH_HIGH_BG;
		}

		/* Background shift registers shift during ticks 322...337. */
		for (cycle = 322; cycle <= 337; cycle++)
			ppu->pre_render_line[cycle] |= EVENT_SHIFT_BG;

		/* Shifters are reloaded during ticks 329 and 337. */
		for (cycle = 329; cycle <= 337; cycle += 8)
			ppu->pre_render_line[cycle] |= EVENT_RELOAD_BG;

		/* Add inc hori(v) updates during ticks 328 and 336. */
		for (cycle = 328; cycle <= 336; cycle += 8)
			ppu->pre_render_line[cycle] |= EVENT_LOOPY_INC_HORI_V;

	/* Cycles 337-340 */
		/* Two bytes are fetched, but the purpose for this is unknown.
		These fetches are 2 PPU cycles each.
			- Nametable byte
			- Nametable byte */
		for (cycle = 337; cycle <= 340; cycle += 4) {
			ppu->pre_render_line[cycle] |= EVENT_FETCH_NT;
			ppu->pre_render_line[cycle + 2] |= EVENT_FETCH_NT;
		}
}

void ppu_build_visible_line(struct ppu *ppu)
{
	int cycle;

	/* Cycle 0 */
		/* This is an idle cycle. The value on the PPU address bus
		during this cycle appears to be the same CHR address that is
		later used to fetch the low background tile byte starting at dot
		5 (possibly calculated during the two unused NT fetches at the
		end of the previous scanline). */

	/* Cycles 1-256 */
		/* Output pixels between ticks 2...257. */
		for (cycle = 2; cycle <= 257; cycle++)
			ppu->visible_line[cycle] |= EVENT_OUTPUT;

		/* The data for each tile is fetched during this phase. Each
		memory access takes 2 PPU cycles to complete, and 4 must be
		performed per tile:
			- Nametable byte
			- Attribute table byte
			- Tile bitmap low
			- Tile bitmap high (+ 8 bytes from tile bitmap low) */
		for (cycle = 1; cycle <= 256; cycle += 8) {
			ppu->visible_line[cycle] |= EVENT_FETCH_NT;
			ppu->visible_line[cycle + 2] |= EVENT_FETCH_AT;
			ppu->visible_line[cycle + 4] |= EVENT_FETCH_LOW_BG;
			ppu->visible_line[cycle + 6] |= EVENT_FETCH_HIGH_BG;
		}

		/* BG and sprite shift registers shift during ticks 2...257. */
		for (cycle = 2; cycle <= 257; cycle++) {
			ppu->visible_line[cycle] |= EVENT_SHIFT_BG;
			ppu->visible_line[cycle] |= EVENT_SHIFT_SPR;
		}

		/* Shifters are reloaded during ticks 9, 17, ..., 257. */
		for (cycle = 9; cycle <= 257; cycle += 8)
			ppu->visible_line[cycle] |= EVENT_RELOAD_BG;

		/* Add inc hori(v) updates during ticks 8, 16, ..., 256. */
		for (cycle = 8; cycle <= 256; cycle += 8)
			ppu->visible_line[cycle] |= EVENT_LOOPY_INC_HORI_V;

		/* Add inc vert(v) update during tick 256. */
		cycle = 256;
		ppu->visible_line[cycle] |= EVENT_LOOPY_INC_VERT_V;

		/* Add hori(v) = hori(t) during tick 257. */
		cycle = 257;
		ppu->visible_line[cycle] |= EVENT_LOOPY_SET_HORI_V;

	/* Cycles 257-320 */
		/* The tile data for the sprites on the next scanline are
		fetched here. Again, each memory access takes 2 PPU cycles to
		complete, and 4 are performed for each of the 8 sprites:
		- Garbage nametable byte
		- Garbage nametable byte
		- Tile bitmap low
		- Tile bitmap high (+ 8 bytes from tile bitmap low) */
		for (cycle = 257; cycle < 320; cycle += 8)
			ppu->visible_line[cycle] |= EVENT_FETCH_SPRITE;

	/* Cycles 321-336 */
		/* This is where the first two tiles for the next scanline are
		fetched, and loaded into the shift registers. Again, each memory
		access takes 2 PPU cycles to complete, and 4 are performed for
		the two tiles:
			- Nametable byte
			- Attribute table byte
			- Tile bitmap low
			- Tile bitmap high (+ 8 bytes from tile bitmap low) */
		for (cycle = 321; cycle <= 336; cycle += 8) {
			ppu->visible_line[cycle] |= EVENT_FETCH_NT;
			ppu->visible_line[cycle + 2] |= EVENT_FETCH_AT;
			ppu->visible_line[cycle + 4] |= EVENT_FETCH_LOW_BG;
			ppu->visible_line[cycle + 6] |= EVENT_FETCH_HIGH_BG;
		}

		/* Background shift registers shift during ticks 322...337. */
		for (cycle = 322; cycle <= 337; cycle++)
			ppu->visible_line[cycle] |= EVENT_SHIFT_BG;

		/* Shifters are reloaded during ticks 329 and 337. */
		for (cycle = 329; cycle <= 337; cycle += 8)
			ppu->visible_line[cycle] |= EVENT_RELOAD_BG;

		/* Add inc hori(v) updates during ticks 328 and 336. */
		for (cycle = 328; cycle <= 336; cycle += 8)
			ppu->visible_line[cycle] |= EVENT_LOOPY_INC_HORI_V;

	/* Cycles 337-340 */
		/* Two bytes are fetched, but the purpose for this is unknown.
		These fetches are 2 PPU cycles each.
			- Nametable byte
			- Nametable byte */
		for (cycle = 337; cycle <= 340; cycle += 4) {
			ppu->visible_line[cycle] |= EVENT_FETCH_NT;
			ppu->visible_line[cycle + 2] |= EVENT_FETCH_NT;
		}

	/* Cycles 1-64 */
		/* Secondary OAM (32-byte buffer for current sprites on
		scanline) is initialized to $FF. */
		cycle = 1;
		ppu->visible_line[cycle] |= EVENT_SEC_OAM_CLEAR;

	/* Cycles 65-256 */
		/* Sprite evaluation. */
		cycle = 65;
		ppu->visible_line[cycle] |= EVENT_SPRITE_EVAL;
}

void ppu_build_vblank_line(struct ppu *ppu)
{
	int cycle;

	/* Cycle 1 */
		/* Add VBLANK set event. */
		cycle = 1;
		ppu->vblank_line[cycle] |= EVENT_VBLANK_SET;
}

void ppu_set_events(struct ppu *ppu)
{
	int v;

	/* Build pre-render, visible, and vertical blanking scanlines */
	ppu_build_pre_render_line(ppu);
	ppu_build_visible_line(ppu);
	ppu_build_vblank_line(ppu);

	/* Pre-render scanline (261)
	This is a dummy scanline, whose sole purpose is to fill the
	shift registers with the data for the first two tiles of the
	next scanline. Although no pixels are rendered for this
	scanline, the PPU still makes the same memory accesses it would
	for a regular scanline. */
	v = 261;
	ppu->events[v] = ppu->pre_render_line;

	/* Visible scanlines (0-239)
	These are the visible scanlines, which contain the graphics to be
	displayed on the screen. This includes the rendering of both the
	background and the sprites. During these scanlines, the PPU is busy
	fetching data, so the program should not access PPU memory during this
	time, unless rendering is turned off. */
	for (v = 0; v <= 239; v++)
		ppu->events[v] = ppu->visible_line;

	/* Post-render scanline (240)
	The PPU just idles during this scanline. Even though accessing PPU
	memory from the program would be safe here, the VBlank flag isn't set
	until after this scanline. */
	v = 240;
	ppu->events[v] = ppu->idle_line;

	/* Vertical blanking lines (241-260)
	The VBlank flag of the PPU is set at tick 1 (the second tick) of
	scanline 241, where the VBlank NMI also occurs. The PPU makes no memory
	accesses during these scanlines, so PPU memory can be freely accessed by
	the program. */
	v = 241;
	ppu->events[v] = ppu->vblank_line;
	for (v = 242; v <= 260; v++)
		ppu->events[v] = ppu->idle_line;
}

void ppu_update_counters(struct ppu *ppu)
{
	/* Increment h and return if end of scanline is not reached */
	if (++ppu->h < NUM_DOTS)
		return;

	/* Reset h */
	ppu->h = 0;

	/* Increment v and return if end of frame is not reached */
	if (++ppu->v < NUM_SCANLINES)
		return;

	/* Reset v and update odd frame flag */
	ppu->v = 0;
	ppu->odd_frame = !ppu->odd_frame;

	/* Skip cycle 0 on odd frames when BG rendering is on */
	if (ppu->odd_frame && ppu->mask.bg_visibility)
		ppu->h++;
}

void ppu_tick(struct ppu *ppu)
{
	int event_mask;
	int pos;
	int num_cycles = 0;

	/* Get event mask for current cycle */
	event_mask = ppu->events[ppu->v][ppu->h];

	/* Loop through all events and fire them */
	while ((pos = bitops_ffs(event_mask))) {
		ppu_events[pos - 1](ppu);
		event_mask &= ~BIT(pos - 1);
	}

	/* Update h/v counters until next event is found */
	do {
		ppu_update_counters(ppu);
		event_mask = ppu->events[ppu->v][ppu->h];
		num_cycles++;
	} while (!event_mask);

	/* Report cycle consumption */
	clock_consume(num_cycles);
}

bool ppu_init(struct controller_instance *instance)
{
	struct ppu *ppu;
	struct video_specs video_specs;
	struct resource *res;

	/* Initialize video frontend */
	video_specs.width = SCREEN_WIDTH;
	video_specs.height = SCREEN_HEIGHT;
	video_specs.fps = SCREEN_REFRESH_RATE;
	if (!video_init(&video_specs))
		return false;

	/* Allocate PPU structure */
	instance->priv_data = calloc(1, sizeof(struct ppu));
	ppu = instance->priv_data;

	/* Add PPU memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	ppu->region.area = res;
	ppu->region.mops = &ppu_mops;
	ppu->region.data = ppu;
	memory_region_add(&ppu->region);

	/* Add palette region */
	res = resource_get("pal",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	ppu->palette_region.area = res;
	ppu->palette_region.mops = &palette_mops;
	ppu->palette_region.data = ppu->palette;
	memory_region_add(&ppu->palette_region);

	/* Save bus ID for later use */
	ppu->bus_id = instance->bus_id;

	/* Get IRQ number */
	res = resource_get("irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	ppu->irq = res->data.irq;

	/* Set up clock */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	ppu->clock.rate = res->data.clk;
	ppu->clock.data = ppu;
	ppu->clock.tick = (clock_tick_t)ppu_tick;
	clock_add(&ppu->clock);

	/* Prepare frame events */
	ppu_set_events(ppu);

	return true;
}

void ppu_reset(struct controller_instance *instance)
{
	struct ppu *ppu = instance->priv_data;

	/* Initialize registers and data */
	ppu->ctrl.value = 0;
	ppu->mask.value = 0;
	ppu->status.value = 0;
	ppu->write_toggle = false;
	ppu->odd_frame = false;
	ppu->h = 0;
	ppu->v = 261;
	ppu->sprite_counter = 0;

	/* Enable clock */
	ppu->clock.enabled = true;
}

void ppu_deinit(struct controller_instance *instance)
{
	video_deinit();
	free(instance->priv_data);
}

CONTROLLER_START(ppu)
	.init = ppu_init,
	.reset = ppu_reset,
	.deinit = ppu_deinit
CONTROLLER_END

