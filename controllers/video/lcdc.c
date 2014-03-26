#include <stdlib.h>
#include <string.h>
#include <bitops.h>
#include <clock.h>
#include <controller.h>
#include <cpu.h>
#include <memory.h>
#include <util.h>
#include <video.h>

/* LCDC registers */
#define NUM_REGS		12
#define CTRL			0
#define STAT			1
#define SCY			2
#define SCX			3
#define LY			4
#define LYC			5
#define DMA			6
#define BGP			7
#define OBP0			8
#define OBP1			9
#define WY			10
#define WX			11

/* LCDC constants */
#define LCD_WIDTH		160
#define LCD_HEIGHT		144
#define NUM_LINES		154
#define NUM_CYCLES_PER_LINE	456
#define DMA_DEST_ADDRESS	0xFE00
#define DMA_TRANSFER_SIZE	160
#define TILE_MAP_ADDRESS_1	0x9800
#define TILE_MAP_ADDRESS_2	0x9C00
#define TILE_WIDTH		8
#define TILE_HEIGHT		8
#define TILE_SIZE		16
#define NUM_TILES_PER_LINE	32
#define TILE_DATA_ADDRESS_1	0x9000
#define TILE_DATA_ADDRESS_2	0x8000
#define WINDOW_OFFSET_X		7

/* LCDC events (sorted by priority) */
#define EVENT_SET_COINCIDENCE	BIT(0)
#define EVENT_MODE_0		BIT(1)
#define EVENT_MODE_1		BIT(2)
#define EVENT_MODE_2		BIT(3)
#define EVENT_MODE_3		BIT(4)

/* Palette macros */
#define R(shade)		(255 - shade * 40 - 70)
#define G(shade)		(255 - shade * 40 - 60)
#define B(shade)		(255 - shade * 40 - 80)

struct ctrl {
	uint8_t bg_display_enable:1;
	uint8_t obj_display_enable:1;
	uint8_t obj_size:1;
	uint8_t bg_tile_map_display_select:1;
	uint8_t bg_and_window_tile_data_select:1;
	uint8_t window_display_enable:1;
	uint8_t window_tile_map_display_select:1;
	uint8_t lcd_display_enable:1;
};

struct stat {
	uint8_t mode_flag:2;
	uint8_t coincidence_flag:1;
	uint8_t mode_0_hblank_interrupt:1;
	uint8_t mode_1_vblank_interrupt:1;
	uint8_t mode_2_oam_interrupt:1;
	uint8_t coincidence_interrupt:1;
	uint8_t reserved:1;
};

struct sprite_flags {
	uint8_t reserved:4;
	uint8_t palette_number:1;
	uint8_t x_flip:1;
	uint8_t y_flip:1;
	uint8_t priority:1;
};

struct sprite {
	uint8_t y_pos;
	uint8_t x_pos;
	uint8_t pattern_number;
	struct sprite_flags flags;
};

struct lcdc {
	union {
		uint8_t regs[NUM_REGS];
		struct {
			struct ctrl ctrl;
			struct stat stat;
			uint8_t scy;
			uint8_t scx;
			uint8_t ly;
			uint8_t lyc;
			uint8_t dma;
			uint8_t bgp;
			uint8_t obp0;
			uint8_t obp1;
			uint8_t wy;
			uint8_t wx;
		};
	};
	int h;
	int v;
	int *events[NUM_LINES];
	int visible_scanline[NUM_CYCLES_PER_LINE];
	int vblank_scanline[NUM_CYCLES_PER_LINE];
	int idle_scanline[NUM_CYCLES_PER_LINE];
	bool line_mask[LCD_WIDTH];
	int bus_id;
	struct clock clock;
	int vblank_irq;
	int lcdc_irq;
};

typedef void (*lcdc_event_t)(struct lcdc *lcdc);

static bool lcdc_init(struct controller_instance *instance);
static void lcdc_deinit(struct controller_instance *instance);
static void lcdc_tick(struct lcdc *lcdc);
static void lcdc_update_counters(struct lcdc *lcdc);
static void lcdc_set_events(struct lcdc *lcdc);
static uint8_t lcdc_readb(struct lcdc *lcdc, address_t address);
static void lcdc_writeb(struct lcdc *lcdc, uint8_t b, address_t address);
static void lcdc_draw_line(struct lcdc *lcdc, bool background);
static void lcdc_set_coincidence(struct lcdc *lcdc);
static void lcdc_mode_0(struct lcdc *lcdc);
static void lcdc_mode_1(struct lcdc *lcdc);
static void lcdc_mode_2(struct lcdc *lcdc);
static void lcdc_mode_3(struct lcdc *lcdc);

static struct mops lcdc_mops = {
	.readb = (readb_t)lcdc_readb,
	.writeb = (writeb_t)lcdc_writeb
};

static lcdc_event_t lcdc_events[] = {
	lcdc_set_coincidence,
	lcdc_mode_0,
	lcdc_mode_1,
	lcdc_mode_2,
	lcdc_mode_3
};

uint8_t lcdc_readb(struct lcdc *lcdc, address_t address)
{
	switch (address) {
	case DMA:
		/* DMA register is write-only */
		return 0;
	default:
		return lcdc->regs[address];
	}
}

void lcdc_writeb(struct lcdc *lcdc, uint8_t b, address_t address)
{
	uint16_t source_addr;
	int i;

	switch (address) {
	case STAT:
		/* Bits 0-2 are read-only so only set bits 3-6 */
		bitops_setb(&lcdc->regs[STAT], 3, 4, bitops_getb(&b, 3, 4));
		break;
	case LY:
		/* Register is read-only */
		break;
	case DMA:
		/* Handle DMA (data byte represents upper 8 bits of source) */
		source_addr = b << 8;
		for (i = 0; i < DMA_TRANSFER_SIZE; i++) {
			b = memory_readb(lcdc->bus_id, source_addr);
			memory_writeb(lcdc->bus_id, b, DMA_DEST_ADDRESS);
		}
		break;
	default:
		lcdc->regs[address] = b;
		break;
	}
}

void lcdc_draw_line(struct lcdc *lcdc, bool background)
{
	bool map_sel;
	int16_t x_off;
	int16_t y_off;
	uint16_t tile_index_base;
	uint16_t tile_index_addr;
	uint16_t tile_data_base;
	uint16_t tile_data_addr;
	uint8_t tile_index;
	uint8_t tile_data[2];
	uint8_t bits[2];
	uint8_t palette_index;
	uint8_t x;
	uint8_t start_x;
	uint8_t sh;
	uint8_t shade;
	struct color color;

	/* Return if window line does not need to be drawn */
	if (!background && (lcdc->wy > lcdc->ly))
		return;

	/* Set starting X coordinate */
	start_x = 0;
	if (!background && (lcdc->wx > WINDOW_OFFSET_X))
		start_x = lcdc->wx - WINDOW_OFFSET_X;

	/* Compute offsets based on background/window selection */
	x_off = background ? lcdc->scx : WINDOW_OFFSET_X - lcdc->wx;
	y_off = background ? lcdc->scy : -lcdc->wy;

	/* Set map selection based on background/window selection */
	map_sel = background ? lcdc->ctrl.bg_tile_map_display_select :
		lcdc->ctrl.window_tile_map_display_select;

	/* Set tile index base depending on map selection and current line */
	tile_index_base = map_sel ? TILE_MAP_ADDRESS_2 : TILE_MAP_ADDRESS_1;
	tile_index_base += ((lcdc->ly + y_off) / TILE_HEIGHT) *
		NUM_TILES_PER_LINE;

	/* Set tile data base depending on data selection and current line */
	tile_data_base = lcdc->ctrl.bg_and_window_tile_data_select ?
		TILE_DATA_ADDRESS_2 : TILE_DATA_ADDRESS_1;
	tile_data_base += ((lcdc->ly + y_off) % TILE_HEIGHT) *
		(TILE_SIZE / TILE_WIDTH);

	/* Draw line */
	for (x = start_x; x < LCD_WIDTH; x++) {
		/* Set tile index address according to current column */
		tile_index_addr = tile_index_base;
		tile_index_addr += (x + x_off) / TILE_WIDTH;

		/* Get tile index from memory */
		tile_index = memory_readb(lcdc->bus_id, tile_index_addr);

		/* Set tile data address according to current tile index and
		data selection bit (first tile map indices are signed) */
		tile_data_addr = tile_data_base;
		tile_data_addr += lcdc->ctrl.bg_and_window_tile_data_select ?
			tile_index * TILE_SIZE : (int8_t)tile_index * TILE_SIZE;

		/* Get tile data from memory */
		tile_data[0] = memory_readb(lcdc->bus_id, tile_data_addr);
		tile_data[1] = memory_readb(lcdc->bus_id, tile_data_addr + 1);

		/* Set data shift based on current column */
		sh = TILE_WIDTH - ((uint8_t)(x + x_off) % TILE_WIDTH) - 1;

		/* Get appropriate data bits and compute palette index */
		bits[0] = bitops_getb(&tile_data[0], sh, 1);
		bits[1] = bitops_getb(&tile_data[1], sh, 1);
		palette_index = bits[0] | (bits[1] << 1);

		/* Compute color based on palette index and draw pixel */
		shade = bitops_getb(&lcdc->bgp, palette_index * 2, 2);
		color.r = R(shade);
		color.g = G(shade);
		color.b = B(shade);
		video_set_pixel(x, lcdc->ly, color);
	}
}

void lcdc_set_coincidence(struct lcdc *lcdc)
{
	/* Set coincidence flag according to current line */
	lcdc->stat.coincidence_flag = (lcdc->ly == lcdc->lyc);

	/* Fire interrupt if needed */
	if (lcdc->stat.coincidence_interrupt && lcdc->stat.coincidence_flag)
		cpu_interrupt(lcdc->lcdc_irq);
}

void lcdc_mode_0(struct lcdc *lcdc)
{
	/* Update mode */
	lcdc->stat.mode_flag = 0;

	/* Draw background if needed */
	if (lcdc->ctrl.bg_display_enable)
		lcdc_draw_line(lcdc, true);

	/* Draw window if needed */
	if (lcdc->ctrl.window_display_enable && (lcdc->wy <= lcdc->ly))
		lcdc_draw_line(lcdc, false);

	/* Fire interrupt if needed */
	if (lcdc->stat.mode_0_hblank_interrupt)
		cpu_interrupt(lcdc->lcdc_irq);
}

void lcdc_mode_1(struct lcdc *lcdc)
{
	/* Update mode */
	lcdc->stat.mode_flag = 1;

	/* Fire VBLANK interrupt */
	cpu_interrupt(lcdc->vblank_irq);

	/* Fire LCDC interrupt if needed as well */
	if (lcdc->stat.mode_1_vblank_interrupt)
		cpu_interrupt(lcdc->lcdc_irq);

	/* Update screen contents */
	video_unlock();
	video_update();
}

void lcdc_mode_2(struct lcdc *lcdc)
{
	/* Update mode */
	lcdc->stat.mode_flag = 2;

	/* Fire interrupt if needed */
	if (lcdc->stat.mode_2_oam_interrupt)
		cpu_interrupt(lcdc->lcdc_irq);

	/* Lock screen at first line */
	if (lcdc->ly == 0)
		video_lock();
}

void lcdc_mode_3(struct lcdc *lcdc)
{
	/* Update mode */
	lcdc->stat.mode_flag = 3;
}

void lcdc_set_events(struct lcdc *lcdc)
{
	int h;
	int v;

	/* Make sure no events are set initially */
	for (h = 0; h < NUM_CYCLES_PER_LINE; h++) {
		lcdc->visible_scanline[h] = 0;
		lcdc->vblank_scanline[h] = 0;
		lcdc->idle_scanline[h] = 0;
	}

	/* Build visible scanline */
	lcdc->visible_scanline[0] = EVENT_SET_COINCIDENCE | EVENT_MODE_2;
	lcdc->visible_scanline[80] = EVENT_MODE_3;
	lcdc->visible_scanline[252] = EVENT_MODE_0;

	/* Build VBLANK scanline */
	lcdc->vblank_scanline[0] |= EVENT_SET_COINCIDENCE | EVENT_MODE_1;

	/* Build idle scanline */
	lcdc->idle_scanline[0] = EVENT_SET_COINCIDENCE;

	/* Build frame events */
	for (v = 0; v <= 143; v++)
		lcdc->events[v] = lcdc->visible_scanline;
	lcdc->events[144] = lcdc->vblank_scanline;
	for (v = 145; v <= 153; v++)
		lcdc->events[v] = lcdc->idle_scanline;
}

void lcdc_update_counters(struct lcdc *lcdc)
{
	/* Increment horizontal counter and return if needed */
	if (++lcdc->h < NUM_CYCLES_PER_LINE)
		return;

	/* Reset horizontal counter */
	lcdc->h = 0;

	/* Increment vertical counter and handle wrapping */
	if (++lcdc->v == NUM_LINES)
		lcdc->v = 0;

	/* Update LY */
	lcdc->ly = lcdc->v;
}

void lcdc_tick(struct lcdc *lcdc)
{
	int event_mask;
	int pos;
	int num_cycles = 0;

	/* Get event mask for current cycle */
	event_mask = lcdc->events[lcdc->v][lcdc->h];

	/* Loop through all events and fire them if LCD is enabled */
	while ((pos = bitops_ffs(event_mask))) {
		if (lcdc->ctrl.lcd_display_enable)
			lcdc_events[pos - 1](lcdc);
		event_mask &= ~BIT(pos - 1);
	}

	/* Update counters until next event is found */
	do {
		lcdc_update_counters(lcdc);
		event_mask = lcdc->events[lcdc->v][lcdc->h];
		num_cycles++;
	} while (!event_mask);

	/* Report cycle consumption */
	clock_consume(num_cycles);
}

bool lcdc_init(struct controller_instance *instance)
{
	struct lcdc *lcdc;
	struct resource *res;

	/* Initialize video frontend */
	if (!video_init(LCD_WIDTH, LCD_HEIGHT))
		return false;

	/* Allocate LCDC structure */
	instance->priv_data = malloc(sizeof(struct lcdc));
	lcdc = instance->priv_data;

	/* Add LCDC memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	memory_region_add(res, &lcdc_mops, lcdc);

	/* Save bus ID for later use */
	lcdc->bus_id = instance->bus_id;

	/* Set up clock */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	lcdc->clock.rate = res->data.clk;
	lcdc->clock.data = lcdc;
	lcdc->clock.tick = (clock_tick_t)lcdc_tick;
	lcdc->clock.enabled = true;
	clock_add(&lcdc->clock);

	/* Get VBLANK IRQ number */
	res = resource_get("vblank",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	lcdc->vblank_irq = res->data.irq;

	/* Get LCDC IRQ number */
	res = resource_get("lcdc",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	lcdc->lcdc_irq = res->data.irq;

	/* Initialize registers and data */
	memset(lcdc->regs, 0, NUM_REGS * sizeof(uint8_t));
	lcdc->h = 0;
	lcdc->v = 0;
	lcdc->ly = 0;
	lcdc->stat.mode_flag = 2;

	/* Prepare frame events */
	lcdc_set_events(lcdc);

	return true;
}

void lcdc_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(lcdc)
	.init = lcdc_init,
	.deinit = lcdc_deinit
CONTROLLER_END

