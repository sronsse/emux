#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
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
#define NUM_DOTS		341
#define NUM_SCANLINES		262
#define OAM_SIZE		256
#define TILE_WIDTH		8
#define TILE_HEIGHT		8
#define PATTERN_TABLE_0_START	0x0000
#define PATTERN_TABLE_1_START	0x1000
#define NAME_TABLE_START	0x2000
#define ATTRIBUTE_TABLE_START	0x23C0
#define PALETTE_START		0x3F00
#define TILE_SIZE		16
#define NUM_CHROMA_VALUES	16
#define NUM_LUMA_VALUES		4

/* PPU events sorted by priority */
#define EVENT_SHIFT_BG		BIT(0)
#define EVENT_RELOAD_BG		BIT(1)
#define EVENT_FETCH_NT		BIT(2)
#define EVENT_FETCH_AT		BIT(3)
#define EVENT_FETCH_LOW_BG	BIT(4)
#define EVENT_FETCH_HIGH_BG	BIT(5)
#define EVENT_VBLANK_SET	BIT(6)
#define EVENT_VBLANK_CLEAR	BIT(7)
#define EVENT_LOOPY_INC_HORI_V	BIT(8)
#define EVENT_LOOPY_INC_VERT_V	BIT(9)
#define EVENT_LOOPY_SET_HORI_V	BIT(10)
#define EVENT_LOOPY_SET_VERT_V	BIT(11)

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
		uint8_t bg_clipping:1;
		uint8_t sprite_cipping:1;
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
	int *events[NUM_SCANLINES];
	int visible_scanline[NUM_DOTS];
	int vblank_scanline[NUM_DOTS];
	int pre_render_scanline[NUM_DOTS];
	int idle_scanline[NUM_DOTS];
	struct ppu_render_data render_data;
	struct clock clock;
	uint8_t oam[OAM_SIZE];
	int bus_id;
	int irq;
};

typedef void (*ppu_event_t)(struct ppu *ppu);

static bool ppu_init(struct controller_instance *instance);
static void ppu_deinit(struct controller_instance *instance);
static void ppu_tick(clock_data_t *data);
static void ppu_update_counters(struct ppu *ppu);
static void ppu_set_events(struct ppu *ppu);
static uint8_t ppu_readb(region_data_t *data, uint16_t address);
static void ppu_writeb(region_data_t *data, uint8_t b, uint16_t address);
static void ppu_shift_bg(struct ppu *ppu);
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

static struct mops ppu_mops = {
	.readb = ppu_readb,
	.writeb = ppu_writeb
};

static ppu_event_t ppu_events[] = {
	ppu_shift_bg,
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
	ppu_loopy_set_vert_v
};

static struct color ppu_palette[NUM_LUMA_VALUES][NUM_CHROMA_VALUES] = {
	{
		{ 128, 128, 128 }, { 0, 61, 166 }, { 0, 18, 176 },
		{ 68, 0, 150 }, { 161, 0, 94 }, { 199, 0, 40 },
		{ 186, 6, 0 }, { 140, 23, 0 }, { 92, 47, 0 },
		{ 16, 69, 0 }, { 5, 74, 0 }, { 0, 71, 46 },
		{ 0, 65, 102 }, { 0, 0, 0 }, { 5, 5, 5 },
		{ 5, 5, 5 }
	},
	{
		{ 199, 199, 199 }, { 0, 119, 255 }, { 33, 85, 255 },
		{ 130, 55, 250 }, { 235, 47, 181 }, { 255, 41, 80 },
		{ 255, 34, 0 }, { 214, 50, 0 }, { 196, 98, 0 },
		{ 53, 128, 0 }, { 5, 143, 0 }, { 0, 138, 85 },
		{ 0, 153, 204 }, { 33, 33, 33 }, { 9, 9, 9 },
		{ 9, 9, 9 }
	},
	{
		{ 255, 255, 255 }, { 15, 215, 255 }, { 105, 162, 255 },
		{ 212, 128, 255 }, { 255, 69, 243 }, { 255, 97, 139 },
		{ 255, 136, 51 }, { 255, 156, 18 }, { 250, 188, 32 },
		{ 159, 227, 14 }, { 43, 240, 53 }, { 12, 240, 164 },
		{ 5, 251, 255 }, { 94, 94, 94 }, { 13, 13, 13 },
		{ 0, 0, 0 }
	},
	{
		{ 255, 255, 255 }, { 166, 252, 255 }, { 179, 236, 255 },
		{ 218, 171, 235 }, { 255, 168, 249 }, { 255, 171, 179 },
		{ 255, 210, 176 }, { 255, 239, 166 }, { 255, 247, 156 },
		{ 215, 232, 149 }, { 166, 237, 175 }, { 162, 242, 218 },
		{ 153, 255, 252 }, { 221, 221, 221 }, { 17, 17, 17 },
		{ 17, 17, 17 }
	}
};

uint8_t ppu_readb(region_data_t *data, uint16_t address)
{
	struct ppu *ppu = data;
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
		if (ppu->vram_addr.value >= PALETTE_START)
			b = ppu->vram_buffer;
		return b;
	default:
		return 0;
	}
}

void ppu_writeb(region_data_t *data, uint8_t b, uint16_t address)
{
	struct ppu *ppu = data;
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

void ppu_shift_bg(struct ppu *ppu)
{
	struct ppu_render_data *r = &ppu->render_data;
	union ppu_palette_entry entry;
	int low;
	int high;
	uint8_t palette;
	uint8_t color;
	int address;
	uint8_t b;
	int x;
	int y;

	/* Return already if rendering is not enabled */
	if ((!ppu->mask.bg_visibility) && (!ppu->mask.sprite_visibility))
		return;

	/* Get palette index */
	low = bitops_getb(&r->shift_at_low, 7 - ppu->fine_x_scroll, 1);
	high = bitops_getb(&r->shift_at_high, 7 - ppu->fine_x_scroll, 1);
	palette = low | (high << 1);

	/* Get pattern color */
	low = bitops_getw(&r->shift_bg_low, 15 - ppu->fine_x_scroll, 1);
	high = bitops_getw(&r->shift_bg_high, 15 - ppu->fine_x_scroll, 1);
	color = low | (high << 1);

	/* Get palette entry (color 0 always redirects to the same address) */
	address = PALETTE_START;
	if (color != 0)
		address += 4 * palette + color;
	entry.value = memory_readb(ppu->bus_id, address);

	/* Shift background and palette shift registers */
	r->shift_bg_low <<= 1;
	r->shift_bg_high <<= 1;
	r->shift_at_low <<= 1;
	r->shift_at_high <<= 1;

	/* Reload palette shift registers */
	b = r->attr_latch;
	r->shift_at_low |= bitops_getb(&b, 0, 1);
	r->shift_at_high |= bitops_getb(&b, 1, 1);

	/* Set pixel based on palette entry */
	x = ppu->h - 2;
	y = ppu->v;
	if ((x < SCREEN_WIDTH) && (y < SCREEN_HEIGHT))
		video_set_pixel(x, y, ppu_palette[entry.luma][entry.chroma]);
}

void ppu_reload_bg(struct ppu *ppu)
{
	struct ppu_render_data *r = &ppu->render_data;

	/* Return already if rendering is not enabled */
	if ((!ppu->mask.bg_visibility) && (!ppu->mask.sprite_visibility))
		return;

	/* Load bitmap data into shift registers (lower 8 bits) */
	bitops_setw(&r->shift_bg_low, 0, 8, r->bg_low);
	bitops_setw(&r->shift_bg_high, 0, 8, r->bg_high);

	/* Load attribute latch */
	r->attr_latch = r->at;
}

void ppu_fetch_nt(struct ppu *ppu)
{
	uint16_t address;
	struct ppu_render_data *r = &ppu->render_data;
	union ppu_vram_address nametable_addr;

	/* Return already if rendering is not enabled */
	if ((!ppu->mask.bg_visibility) && (!ppu->mask.sprite_visibility))
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
	uint16_t address;
	struct ppu_render_data *r = &ppu->render_data;
	union ppu_attribute_address attr_addr;
	union ppu_attribute at;
	uint8_t b;
	bool right;
	bool bottom;

	/* Return already if rendering is not enabled */
	if ((!ppu->mask.bg_visibility) && (!ppu->mask.sprite_visibility))
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
	uint16_t address;
	struct ppu_render_data *r = &ppu->render_data;

	/* Return already if rendering is not enabled */
	if ((!ppu->mask.bg_visibility) && (!ppu->mask.sprite_visibility))
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
	uint16_t address;
	struct ppu_render_data *r = &ppu->render_data;

	/* Return already if rendering is not enabled */
	if ((!ppu->mask.bg_visibility) && (!ppu->mask.sprite_visibility))
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
	ppu->status.sprite_0_hit = 0;
	video_lock();
}

void ppu_loopy_inc_hori_v(struct ppu *ppu)
{
	/* Return already if rendering is not enabled */
	if ((!ppu->mask.bg_visibility) && (!ppu->mask.sprite_visibility))
		return;

	/* Increment coarse X */
	ppu->vram_addr.coarse_x_scroll++;

	/* Toggle horizontal nametable on overflow */
	if (ppu->vram_addr.coarse_x_scroll == 0)
		ppu->vram_addr.h_nametable ^= 1;
}

void ppu_loopy_inc_vert_v(struct ppu *ppu)
{
	/* Return already if rendering is not enabled */
	if ((!ppu->mask.bg_visibility) && (!ppu->mask.sprite_visibility))
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
	/* Return already if rendering is not enabled */
	if ((!ppu->mask.bg_visibility) && (!ppu->mask.sprite_visibility))
		return;

	/* Copy horizontal position from t to v */
	ppu->vram_addr.coarse_x_scroll = ppu->temp_vram_addr.coarse_x_scroll;
	ppu->vram_addr.h_nametable = ppu->temp_vram_addr.h_nametable;
}

void ppu_loopy_set_vert_v(struct ppu *ppu)
{
	/* Return already if rendering is not enabled */
	if ((!ppu->mask.bg_visibility) && (!ppu->mask.sprite_visibility))
		return;

	/* Copy vertical position from t to v */
	ppu->vram_addr.coarse_y_scroll = ppu->temp_vram_addr.coarse_y_scroll;
	ppu->vram_addr.v_nametable = ppu->temp_vram_addr.v_nametable;
	ppu->vram_addr.fine_y_scroll = ppu->temp_vram_addr.fine_y_scroll;
}

void ppu_set_events(struct ppu *ppu)
{
	int h;
	int v;

	/* Make sure no events are set initially */
	for (h = 0; h < NUM_DOTS; h++) {
		ppu->visible_scanline[h] = 0;
		ppu->vblank_scanline[h] = 0;
		ppu->pre_render_scanline[h] = 0;
		ppu->idle_scanline[h] = 0;
	}

	/* Build visible scanline */
	for (h = 1; h <= 255; h += 8) {
		ppu->visible_scanline[h] |= EVENT_FETCH_NT;
		ppu->visible_scanline[h + 2] |= EVENT_FETCH_AT;
		ppu->visible_scanline[h + 4] |= EVENT_FETCH_LOW_BG;
		ppu->visible_scanline[h + 6] |= EVENT_FETCH_HIGH_BG;
		ppu->visible_scanline[h + 7] |= EVENT_LOOPY_INC_HORI_V;
		ppu->visible_scanline[h + 8] |= EVENT_RELOAD_BG;
	}
	for (h = 2; h <= 257; h++)
		ppu->visible_scanline[h] |= EVENT_SHIFT_BG;
	ppu->visible_scanline[256] |= EVENT_LOOPY_INC_VERT_V;
	ppu->visible_scanline[257] |= EVENT_LOOPY_SET_HORI_V;
	for (h = 321; h <= 335; h += 8) {
		ppu->visible_scanline[h] |= EVENT_FETCH_NT;
		ppu->visible_scanline[h + 2] |= EVENT_FETCH_AT;
		ppu->visible_scanline[h + 4] |= EVENT_FETCH_LOW_BG;
		ppu->visible_scanline[h + 6] |= EVENT_FETCH_HIGH_BG;
		ppu->visible_scanline[h + 7] |= EVENT_LOOPY_INC_HORI_V;
		ppu->visible_scanline[h + 8] |= EVENT_RELOAD_BG;
	}
	for (h = 322; h <= 337; h++)
		ppu->visible_scanline[h] |= EVENT_SHIFT_BG;
	ppu->visible_scanline[337] |= EVENT_FETCH_NT;
	ppu->visible_scanline[339] |= EVENT_FETCH_NT;

	/* Build VBLANK scanline */
	ppu->vblank_scanline[1] |= EVENT_VBLANK_SET;

	/* Build pre-render scanline */
	for (h = 1; h <= 255; h += 8) {
		ppu->pre_render_scanline[h] |= EVENT_FETCH_NT;
		ppu->pre_render_scanline[h + 2] |= EVENT_FETCH_AT;
		ppu->pre_render_scanline[h + 4] |= EVENT_FETCH_LOW_BG;
		ppu->pre_render_scanline[h + 6] |= EVENT_FETCH_HIGH_BG;
		ppu->pre_render_scanline[h + 7] |= EVENT_LOOPY_INC_HORI_V;
		ppu->pre_render_scanline[h + 8] |= EVENT_RELOAD_BG;
	}
	for (h = 2; h <= 257; h++)
		ppu->pre_render_scanline[h] |= EVENT_SHIFT_BG;
	ppu->pre_render_scanline[1] |= EVENT_VBLANK_CLEAR;
	ppu->pre_render_scanline[256] |= EVENT_LOOPY_INC_VERT_V;
	ppu->pre_render_scanline[257] |= EVENT_LOOPY_SET_HORI_V;
	for (h = 280; h <= 304; h++)
		ppu->pre_render_scanline[h] |= EVENT_LOOPY_SET_VERT_V;
	for (h = 321; h <= 335; h += 8) {
		ppu->pre_render_scanline[h] |= EVENT_FETCH_NT;
		ppu->pre_render_scanline[h + 2] |= EVENT_FETCH_AT;
		ppu->pre_render_scanline[h + 4] |= EVENT_FETCH_LOW_BG;
		ppu->pre_render_scanline[h + 6] |= EVENT_FETCH_HIGH_BG;
		ppu->pre_render_scanline[h + 7] |= EVENT_LOOPY_INC_HORI_V;
		ppu->pre_render_scanline[h + 8] |= EVENT_RELOAD_BG;
	}
	for (h = 322; h <= 337; h++)
		ppu->pre_render_scanline[h] |= EVENT_SHIFT_BG;
	ppu->pre_render_scanline[337] |= EVENT_FETCH_NT;
	ppu->pre_render_scanline[339] |= EVENT_FETCH_AT;

	/* Build frame events */
	for (v = 0; v <= 239; v++)
		ppu->events[v] = ppu->visible_scanline;
	ppu->events[240] = ppu->idle_scanline;
	ppu->events[241] = ppu->vblank_scanline;
	for (v = 242; v <= 260; v++)
		ppu->events[v] = ppu->idle_scanline;
	ppu->events[261] = ppu->pre_render_scanline;
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

void ppu_tick(clock_data_t *data)
{
	struct ppu *ppu = data;
	int event_mask;
	int pos;
	int num_cycles = 0;

	/* Get event mask for current cycle */
	event_mask = ppu->events[ppu->v][ppu->h];

	/* Loop through all events and fire them */
	while ((pos = ffs(event_mask))) {
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
	struct resource *res;

	/* Initialize video frontend */
	if (!video_init(SCREEN_WIDTH, SCREEN_HEIGHT))
		return false;

	/* Allocate PPU structure */
	instance->priv_data = malloc(sizeof(struct ppu));
	ppu = instance->priv_data;

	/* Add PPU memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	memory_region_add(res, &ppu_mops, instance->priv_data);

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
	ppu->clock.tick = ppu_tick;
	clock_add(&ppu->clock);

	/* Initialize registers and data */
	ppu->ctrl.value = 0;
	ppu->mask.value = 0;
	ppu->status.value = 0;
	ppu->write_toggle = false;
	ppu->odd_frame = false;
	ppu->h = 0;
	ppu->v = 261;

	/* Prepare frame events */
	ppu_set_events(ppu);

	return true;
}

void ppu_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(ppu)
	.init = ppu_init,
	.deinit = ppu_deinit
CONTROLLER_END

