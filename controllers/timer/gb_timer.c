#include <stdlib.h>
#include <clock.h>
#include <controller.h>
#include <cpu.h>
#include <memory.h>

/* Register offsets */
#define DIV		0
#define TIMA		1
#define TMA		2
#define TAC		3
#define NUM_REGS	4

/* Dividers */
#define TIMA_DIV_0	1024
#define TIMA_DIV_1	16
#define TIMA_DIV_2	64
#define TIMA_DIV_3	256

union tac {
	uint8_t value;
	struct {
		uint8_t input_clock_select:2;
		uint8_t timer_enable:1;
		uint8_t reserved:5;
	};
};

struct timer {
	union {
		uint8_t regs[NUM_REGS];
		struct {
			uint8_t div;
			uint8_t tima;
			uint8_t tma;
			union tac tac;
		};
	};
	struct region region;
	struct clock div_clock;
	struct clock tima_clock;
	int irq;
};

static bool timer_init(struct controller_instance *instance);
static void timer_reset(struct controller_instance *instance);
static void timer_deinit(struct controller_instance *instance);
static uint8_t timer_readb(struct timer *timer, address_t address);
static void timer_writeb(struct timer *timer, uint8_t b, address_t address);
static void div_tick(struct timer *timer);
static void tima_tick(struct timer *timer);

static int tima_divs[] = {
	TIMA_DIV_0,
	TIMA_DIV_1,
	TIMA_DIV_2,
	TIMA_DIV_3
};

static struct mops timer_mops = {
	.readb = (readb_t)timer_readb,
	.writeb = (writeb_t)timer_writeb
};

uint8_t timer_readb(struct timer *timer, address_t address)
{
	/* Read requested register */
	return timer->regs[address];
}

void timer_writeb(struct timer *timer, uint8_t b, address_t address)
{
	union tac tac;

	switch (address) {
	case DIV:
		/* Reset register */
		timer->div = 0;
		break;
	case TIMA:
	case TMA:
		/* Write register */
		timer->regs[address] = b;
		break;
	case TAC:
		/* Write to temporary register resetting reserved bits */
		tac.value = b;
		tac.reserved = 0;

		/* Save register */
		timer->tac.value = tac.value;

		/* Enable/disable TIMA clock */
		timer->tima_clock.enabled = timer->tac.timer_enable;
		break;
	}
}

void div_tick(struct timer *timer)
{
	/* Increment divider register, consume one cycle */
	timer->div++;
	clock_consume(1);
}

void tima_tick(struct timer *timer)
{
	int num_cycles;

	/* Increment timer counter and handle overflow */
	if (timer->tima++ == 0xFF) {
		/* Reset counter to modulo value */
		timer->tima = timer->tma;

		/* Interrupt CPU */
		cpu_interrupt(timer->irq);
	}

	/* Consume clock cycles (based on TAC register) */
	num_cycles = tima_divs[timer->tac.input_clock_select];
	clock_consume(num_cycles);
}

bool timer_init(struct controller_instance *instance)
{
	struct timer *timer;
	struct resource *res;

	/* Allocate timer structure */
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

	/* Add divider clock */
	res = resource_get("div_clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	timer->div_clock.rate = res->data.clk;
	timer->div_clock.data = timer;
	timer->div_clock.tick = (clock_tick_t)div_tick;
	clock_add(&timer->div_clock);

	/* Add timer counter clock */
	res = resource_get("tima_clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	timer->tima_clock.rate = res->data.clk;
	timer->tima_clock.data = timer;
	timer->tima_clock.tick = (clock_tick_t)tima_tick;
	clock_add(&timer->tima_clock);

	/* Save IRQ number */
	res = resource_get("irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	timer->irq = res->data.irq;

	return true;
}

void timer_reset(struct controller_instance *instance)
{
	struct timer *timer = instance->priv_data;

	/* Initialize timer registers */
	timer->div = 0;
	timer->tima = 0;
	timer->tma = 0;
	timer->tac.value = 0;

	/* Set initial clock states */
	timer->div_clock.enabled = true;
	timer->tima_clock.enabled = false;
}

void timer_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(gb_timer)
	.init = timer_init,
	.reset = timer_reset,
	.deinit = timer_deinit
CONTROLLER_END

