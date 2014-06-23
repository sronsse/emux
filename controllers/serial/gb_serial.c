#include <stdlib.h>
#include <clock.h>
#include <controller.h>
#include <cpu.h>
#include <memory.h>

/* Register offsets */
#define SB		0
#define SC		1
#define NUM_REGS	2

#define EXTERNAL_CLOCK	0
#define INTERNAL_CLOCK	1

union sc {
	uint8_t value;
	struct {
		uint8_t shift_clock:1;
		uint8_t clock_speed:1;
		uint8_t reserved:5;
		uint8_t transfer_start_flag:1;
	};
};

struct serial {
	union {
		uint8_t regs[NUM_REGS];
		struct {
			uint8_t sb;
			union sc sc; 
		};
	};
	struct region region;
	struct clock clock;
	int irq;
};

static bool serial_init(struct controller_instance *instance);
static void serial_reset(struct controller_instance *instance);
static void serial_deinit(struct controller_instance *instance);
static uint8_t serial_readb(struct serial *serial, address_t address);
static void serial_writeb(struct serial *serial, uint8_t b, address_t address);
static void serial_tick(struct serial *serial);

static struct mops serial_mops = {
	.readb = (readb_t)serial_readb,
	.writeb = (writeb_t)serial_writeb
};

uint8_t serial_readb(struct serial *serial, address_t address)
{
	/* Read requested register */
	return serial->regs[address];
}

void serial_writeb(struct serial *serial, uint8_t b, address_t address)
{
	union sc sc;

	switch (address) {
	case SB:
		/* Write register */
		serial->sb = b;
		break;
	case SC:
		/* Reset reserved bits and save register */
		sc.value = b;
		sc.reserved = 0;
		serial->sc = sc;

		/* Enable/disable clock based on transfer state and type */
		serial->clock.enabled = serial->sc.transfer_start_flag &&
			(serial->sc.shift_clock == INTERNAL_CLOCK);
		break;
	}
}

void serial_tick(struct serial *serial)
{
	/* Simulate bit transfer */
	serial->sb = 0xFF;

	/* Reset transfer start flag */
	serial->sc.transfer_start_flag = 0;

	/* Interrupt CPU */
	cpu_interrupt(serial->irq);

	/* Consume one clock cycle and disable clock */
	clock_consume(1);
	serial->clock.enabled = false;
}

bool serial_init(struct controller_instance *instance)
{
	struct serial *serial;
	struct resource *res;

	/* Allocate serial structure */
	instance->priv_data = malloc(sizeof(struct serial));
	serial = instance->priv_data;

	/* Set up memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	serial->region.area = res;
	serial->region.mops = &serial_mops;
	serial->region.data = serial;
	memory_region_add(&serial->region);

	/* Add clock */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	serial->clock.rate = res->data.clk;
	serial->clock.data = serial;
	serial->clock.tick = (clock_tick_t)serial_tick;
	clock_add(&serial->clock);

	/* Save IRQ number */
	res = resource_get("irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	serial->irq = res->data.irq;

	return true;
}

void serial_reset(struct controller_instance *instance)
{
	struct serial *serial = instance->priv_data;

	/* Initialize registers */
	serial->sb = 0;
	serial->sc.shift_clock = INTERNAL_CLOCK;
	serial->sc.clock_speed = 0;
	serial->sc.transfer_start_flag = 0;

	/* Disable clock by default */
	serial->clock.enabled = false;
}

void serial_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(gb_serial)
	.init = serial_init,
	.reset = serial_reset,
	.deinit = serial_deinit
CONTROLLER_END

