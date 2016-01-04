#include <string.h>
#include <clock.h>
#include <controller.h>
#include <cpu.h>
#include <event.h>
#include <memory.h>

#define NUM_TIMERS	3
#define COUNTER_VALUE	0
#define COUNTER_MODE	1
#define COUNTER_TARGET	2

struct counter_value {
	uint32_t val:16;
	uint32_t unused:16;
};

struct counter_mode {
	uint32_t sync_enable:1;
	uint32_t sync_mode:2;
	uint32_t reset_counter:1;
	uint32_t irq_counter_eq_target:1;
	uint32_t irq_counter_eq_ffff:1;
	uint32_t irq_once_repeat_mode:1;
	uint32_t irq_pulse_toggle_mode:1;
	uint32_t clock_src:2;
	uint32_t int_request:1;
	uint32_t reached_target:1;
	uint32_t reached_ffff:1;
	uint32_t unknown:3;
	uint32_t unused:16;
};

struct counter_target {
	uint32_t val:16;
	uint32_t unused:16;
};

union regs {
	uint32_t raw[4];
	struct {
		struct counter_value counter_value;
		struct counter_mode counter_mode;
		struct counter_target counter_target;
		uint32_t unused;
	};
};

struct instance {
	union regs regs;
	bool running;
	bool interrupt;
	int step;
	int irq;
};

struct timer {
	struct instance instances[NUM_TIMERS];
	struct region region;
	struct clock clock;
};

static bool timer_init(struct controller_instance *instance);
static void timer_deinit(struct controller_instance *instance);
static void timer_tick(struct timer *timer);
static uint16_t timer_readw(struct timer *timer, address_t a);
static void timer_writew(struct timer *timer, uint16_t w, address_t a);
static uint32_t timer_readl(struct timer *timer, address_t a);
static void timer_writel(struct timer *timer, uint32_t l, address_t a);
static void instance_tick(struct instance *instance);
static void dot_clk_event(struct timer *timer);
static void hblank_start_event(struct timer *timer);
static void hblank_end_event(struct timer *timer);
static void vblank_start_event(struct timer *timer);
static void vblank_end_event(struct timer *timer);

static struct mops timer_mops = {
	.readw = (readw_t)timer_readw,
	.writew = (writew_t)timer_writew,
	.readl = (readl_t)timer_readl,
	.writel = (writel_t)timer_writel
};

uint16_t timer_readw(struct timer *timer, address_t a)
{
	/* Call regular read method */
	return timer_readl(timer, a);
}

void timer_writew(struct timer *timer, uint16_t w, address_t a)
{
	/* Call regular write method */
	timer_writel(timer, w, a);
}

uint32_t timer_readl(struct timer *timer, address_t a)
{
	struct instance *instance;
	int index;
	int reg;
	uint32_t l;

	/* Get appropriate instance and register */
	index = a / sizeof(union regs);
	reg = (a % sizeof(union regs)) / sizeof(uint32_t);

	/* Get corresponding instance */
	instance = &timer->instances[index];

	/* Read register */
	l = instance->regs.raw[reg];

	/* Reset reached value bits when counter mode is read */
	if (reg == COUNTER_MODE) {
		instance->regs.counter_mode.reached_target = 0;
		instance->regs.counter_mode.reached_ffff = 0;
	}

	/* Return contents */
	return l;
}

void timer_writel(struct timer *timer, uint32_t l, address_t a)
{
	struct instance *instance;
	int index;
	int reg;

	/* Get appropriate instance and register */
	index = a / sizeof(union regs);
	reg = (a % sizeof(union regs)) / sizeof(uint32_t);

	/* Get corresponding instance */
	instance = &timer->instances[index];

	/* Write corresponding register */
	instance->regs.raw[reg] = l;

	/* Return already on counter value and target writes */
	if ((reg == COUNTER_VALUE) || (reg == COUNTER_TARGET))
		return;

	/* Reset counter value */
	instance->regs.counter_value.val = 0;

	/* In one-shot mode, the IRQ is pulsed/toggled only once (one-shot mode
	doesn't stop the counter, it just suppresses any further IRQs until a
	new write to the Mode register occurs. */
	if (!instance->regs.counter_mode.irq_once_repeat_mode)
		instance->interrupt = false;

	/* Enable timer if synchronization is disabled */
	if (!instance->regs.counter_mode.sync_enable)
		instance->running = true;

	/* Set interrupt request bit (1 = no) */
	instance->regs.counter_mode.int_request = 1;

	/* Handle synchronization mode for timer 2 */
	if ((index == 2) && instance->regs.counter_mode.sync_enable)
		switch (instance->regs.counter_mode.sync_mode) {
		case 0:
		case 3:
			/* Stop counter at current value */
			instance->running = false;
			break;
		case 1:
		case 2:
			/* Free run (same as synchronization disabled) */
			instance->running = true;
			break;
	}
}

void dot_clk_event(struct timer *timer)
{
	struct instance *instance0 = &timer->instances[0];
	int clk_src;
	bool tick;

	/* Return already if timer 0 is not running */
	if (!instance0->running)
		return;

	/* Check if timer 0 needs to be ticked (based on clock source) */
	clk_src = instance0->regs.counter_mode.clock_src;
	tick = ((clk_src == 1) || (clk_src == 3));

	/* Tick timer if needed */
	if (tick)
		instance_tick(instance0);
}

void hblank_start_event(struct timer *timer)
{
	struct instance *instance0 = &timer->instances[0];
	struct instance *instance1 = &timer->instances[1];
	int clk_src;
	bool tick;

	/* Handle synchronization mode for timer 0 */
	if (instance0->regs.counter_mode.sync_enable)
		switch (instance0->regs.counter_mode.sync_mode) {
		case 0:
			/* Pause counter */
			instance0->running = false;
			return;
		case 1:
			/* Reset counter to 0 */
			instance0->regs.counter_value.val = 0;
			break;
		case 2:
			/* Reset counter to 0 and let counter run */
			instance0->regs.counter_value.val = 0;
			instance0->running = true;
			break;
		case 3:
			/* Switch to free run */
			instance0->running = true;
			break;
		}

	/* Return already if timer 1 is not running */
	if (!instance1->running)
		return;

	/* Check if timer 1 needs to be ticked (based on clock source) */
	clk_src = instance1->regs.counter_mode.clock_src;
	tick = ((clk_src == 1) || (clk_src == 3));

	/* Tick timer if needed */
	if (tick)
		instance_tick(instance1);
}

void hblank_end_event(struct timer *timer)
{
	struct instance *instance0 = &timer->instances[0];

	/* Handle synchronization mode for timer 0 */
	if (instance0->regs.counter_mode.sync_enable)
		switch (instance0->regs.counter_mode.sync_mode) {
		case 0:
			/* Let counter run */
			instance0->running = true;
			return;
		case 2:
			/* Pause counter */
			instance0->running = false;
			break;
		}
}

void vblank_start_event(struct timer *timer)
{
	struct instance *instance1 = &timer->instances[1];

	/* Handle synchronization mode for timer 1 */
	if (instance1->regs.counter_mode.sync_enable)
		switch (instance1->regs.counter_mode.sync_mode) {
		case 0:
			/* Pause counter */
			instance1->running = false;
			return;
		case 1:
			/* Reset counter to 0 */
			instance1->regs.counter_value.val = 0;
			break;
		case 2:
			/* Reset counter to 0 and let counter run */
			instance1->regs.counter_value.val = 0;
			instance1->running = true;
			break;
		case 3:
			/* Switch to free run */
			instance1->running = true;
			break;
		}
}

void vblank_end_event(struct timer *timer)
{
	struct instance *instance1 = &timer->instances[1];

	/* Handle synchronization mode for timer 1 */
	if (instance1->regs.counter_mode.sync_enable)
		switch (instance1->regs.counter_mode.sync_mode) {
		case 0:
			/* Let counter run */
			instance1->running = true;
			return;
		case 2:
			/* Pause counter */
			instance1->running = false;
			break;
		}
}

void instance_tick(struct instance *instance)
{
	struct counter_value *cv = &instance->regs.counter_value;
	struct counter_mode *cm = &instance->regs.counter_mode;
	struct counter_target *ct = &instance->regs.counter_target;
	bool reset;
	bool interrupt = false;

	/* When the target flag is set (bit 3 of the mode register), the counter
	increments up to (including) the selected target value, and does then
	restarts at 0. Otherwise, simply increment the counter. */
	reset = (cm->reset_counter && (cv->val == ct->val));
	cv->val = reset ? 0 : cv->val + 1;

	/* Check if counter value matches target */
	if (cv->val == ct->val) {
		/* Set reached target bit */
		cm->reached_target = 1;

		/* Check if interrupt condition is met */
		interrupt |= cm->irq_counter_eq_target;
	}

	/* Check if counter value matches 0xFFFF */
	if (cv->val == 0xFFFF) {
		/* Set reached target bit */
		cm->reached_ffff = 1;

		/* Check if interrupt condition is met */
		interrupt |= cm->irq_counter_eq_ffff;
	}

	/* Leave already if interrupt condition is not met */
	if (!interrupt)
		return;

	/* Return if one shot mode is on and interrupt already occurred */
	if (!cm->irq_once_repeat_mode && instance->interrupt)
		return;

	/* Toggle interrupt request bit */
	cm->int_request ^= 1;

	/* Normally, pulse mode should be used (bit 10 is permanently set,
	except for a few clock cycles when an IRQ occurs). In toggle mode,
	bit 10 is set after writing to the mode register, and becomes inverted
	on each IRQ (in one-shot mode, it remains zero after the IRQ) (in repeat
	mode it inverts bit 10 on each IRQ, so IRQ 4/5/6 are triggered only each
	second time, ie. when bit 10 changes from 1 to 0). */
	if (!cm->int_request) {
		/* Flag interrupt and interrupt CPU */
		instance->interrupt = true;
		cpu_interrupt(instance->irq);

		/* Reset interrupt request bit in pulse mode */
		if (!cm->irq_pulse_toggle_mode)
			cm->int_request = 1;
	}
}

void timer_tick(struct timer *timer)
{
	struct instance *instance;
	bool tick;
	int clk_src;
	int i;

	/* Tick enabled timers */
	for (i = 0; i < NUM_TIMERS; i++) {
		/* Get corresponding instance */
		instance = &timer->instances[i];

		/* Skip timer if not running */
		if (!instance->running)
			continue;

		/* Check if instance needs ticking (and handle timer 2 step) */
		clk_src = instance->regs.counter_mode.clock_src;
		switch (i) {
		case 0:
			/* 0 or 2 = system clock, 1 or 3 = dot clock */
			tick = ((clk_src == 0) || (clk_src == 2));
			break;
		case 1:
			/* 0 or 2 = system clock, 1 or 3 = HBLANK */
			tick = ((clk_src == 0) || (clk_src == 2));
			break;
		case 2:
			/* 0 or 1 = system clock, 2 or 3 = system clock / 8 */
			tick = ((clk_src == 0) || (clk_src == 1));
			tick |= (instance->step == 0);

			/* Handle step and overflow */
			if (((clk_src == 2) || (clk_src == 3)) &&
				(++instance->step == 8))
				instance->step = 0;
			break;
		}

		/* Tick timer if required */
		if (tick)
			instance_tick(instance);
	}

	/* Consume a single cycle */
	clock_consume(1);
}

bool timer_init(struct controller_instance *instance)
{
	struct timer *timer;
	struct resource *res;

	/* Allocate private structure */
	instance->priv_data = calloc(1, sizeof(struct timer));
	timer = instance->priv_data;

	/* Set up memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	timer->region.area = res;
	timer->region.mops = &timer_mops;
	timer->region.data = timer;
	memory_region_add(&timer->region);

	/* Get timer 0 IRQ */
	res = resource_get("tmr0_irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	timer->instances[0].irq = res->data.irq;

	/* Get timer 1 IRQ */
	res = resource_get("tmr1_irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	timer->instances[1].irq = res->data.irq;

	/* Get timer 2 IRQ */
	res = resource_get("tmr2_irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	timer->instances[2].irq = res->data.irq;

	/* Set up clock */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	timer->clock.rate = res->data.clk;
	timer->clock.data = timer;
	timer->clock.tick = (clock_tick_t)timer_tick;
	clock_add(&timer->clock);

	/* Register to GPU events */
	event_add("dot_clk", (event_callback_t)dot_clk_event, timer);
	event_add("hblank_start", (event_callback_t)hblank_start_event, timer);
	event_add("hblank_end", (event_callback_t)hblank_end_event, timer);
	event_add("vblank_start", (event_callback_t)vblank_start_event, timer);
	event_add("vblank_end", (event_callback_t)vblank_end_event, timer);

	return true;
}

void timer_reset(struct controller_instance *instance)
{
	struct timer *timer = instance->priv_data;
	int i;

	/* Reset registers and states of all timer instances */
	for (i = 0; i < NUM_TIMERS; i++) {
		memset(&timer->instances[i].regs, 0, sizeof(union regs));
		timer->instances[i].running = true;
		timer->instances[i].interrupt = false;
		timer->instances[i].step = 0;
	}

	/* Enable clock */
	timer->clock.enabled = true;
}

void timer_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(psx_timer)
	.init = timer_init,
	.reset = timer_reset,
	.deinit = timer_deinit
CONTROLLER_END

