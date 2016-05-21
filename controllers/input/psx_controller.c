#include <clock.h>
#include <controller.h>
#include <cpu.h>
#include <list.h>
#include <memory.h>
#include <util.h>
#include "psx_controller.h"

#define JOY_TX_DATA	0x00
#define JOY_RX_DATA	0x00
#define JOY_STAT	0x04
#define JOY_MODE	0x08
#define JOY_CTRL	0x0A
#define JOY_BAUD	0x0E

#define TX_FIFO_SIZE	2
#define RX_FIFO_SIZE	8

#define MUL1A		0
#define MUL1B		1
#define MUL16		2
#define MUL64		3

#define CHAR_LEN_5BITS	0
#define CHAR_LEN_6BITS	1
#define CHAR_LEN_7BITS	2
#define CHAR_LEN_8BITS	3

#define DEFAULT_BAUD	0x0088

union joy_tx_data {
	uint32_t raw;
	struct {
		uint32_t data:8;
		uint32_t unused:24;
	};
};

union joy_rx_data {
	uint32_t raw;
	struct {
		uint32_t data:8;
		uint32_t preview_1:8;
		uint32_t preview_2:8;
		uint32_t preview_3:8;
	};
};

union joy_stat {
	uint32_t raw;
	struct {
		uint32_t tx_ready_1:1;
		uint32_t rx_fifo_not_empty:1;
		uint32_t tx_ready_2:1;
		uint32_t rx_parity_error:1;
		uint32_t unknown_1:1;
		uint32_t unknown_2:1;
		uint32_t unknown_3:1;
		uint32_t ack_input_lvl:1;
		uint32_t unknown_4:1;
		uint32_t int_req:1;
		uint32_t unknown_5:1;
		uint32_t br_timer:21;
	};
};

union joy_mode {
	uint16_t raw;
	struct {
		uint16_t br_reload_factor:2;
		uint16_t char_len:2;
		uint16_t parity_en:1;
		uint16_t parity_type:1;
		uint16_t unknown_1:1;
		uint16_t destroy_received:1;
		uint16_t unknown_2:1;
	};
};

union joy_ctrl {
	uint16_t raw;
	struct {
		uint16_t tx_en:1;
		uint16_t joyn_output:1;
		uint16_t rx_en:1;
		uint16_t unknown_1:1;
		uint16_t acknowledge:1;
		uint16_t unknown_2:1;
		uint16_t reset:1;
		uint16_t unused_1:1;
		uint16_t rx_int_mode:2;
		uint16_t tx_int_en:1;
		uint16_t rx_int_en:1;
		uint16_t ack_int_en:1;
		uint16_t desired_slot_num:1;
		uint16_t unused_2:2;
	};
};

struct fifo {
	uint8_t *data;
	int size;
	int pos;
	int num;
};

struct psx_ctrl {
	union joy_stat joy_stat;
	union joy_mode joy_mode;
	union joy_ctrl joy_ctrl;
	uint16_t joy_baud;
	uint8_t tx_buffer;
	bool start;
	bool tx_en_latch;
	bool bit_clk_state;
	int bit_num;
	struct list_link *peripherals;
	struct fifo tx_fifo;
	struct fifo rx_fifo;
	struct region region;
	struct clock clock;
	int irq;
};

static bool psx_ctrl_init(struct controller_instance *instance);
static void psx_ctrl_reset(struct controller_instance *instance);
static void psx_ctrl_deinit(struct controller_instance *instance);
static void psx_ctrl_tick(struct psx_ctrl *ctrl);
static void psx_ctrl_handle_interrupts(struct psx_ctrl *ctrl);
static void psx_ctrl_transfer(struct psx_ctrl *ctrl);
static void psx_ctrl_reload_timer(struct psx_ctrl *ctrl);
static uint8_t psx_ctrl_readb(struct psx_ctrl *ctrl, address_t a);
static uint16_t psx_ctrl_readw(struct psx_ctrl *ctrl, address_t a);
static uint32_t psx_ctrl_readl(struct psx_ctrl *ctrl, address_t a);
static void psx_ctrl_writeb(struct psx_ctrl *ctrl, uint8_t b, address_t a);
static void psx_ctrl_writew(struct psx_ctrl *ctrl, uint16_t w, address_t a);
static void psx_ctrl_writel(struct psx_ctrl *ctrl, uint32_t l, address_t a);
static bool fifo_enqueue(struct fifo *fifo, uint8_t data);
static bool fifo_dequeue(struct fifo *fifo, uint8_t *data);

static struct mops psx_ctrl_mops = {
	.readb = (readb_t)psx_ctrl_readb,
	.readw = (readw_t)psx_ctrl_readw,
	.readl = (readl_t)psx_ctrl_readl,
	.writeb = (writeb_t)psx_ctrl_writeb,
	.writew = (writew_t)psx_ctrl_writew,
	.writel = (writel_t)psx_ctrl_writel
};

uint8_t psx_ctrl_readb(struct psx_ctrl *ctrl, address_t address)
{
	/* Call main read handler */
	return psx_ctrl_readl(ctrl, address);
}

uint16_t psx_ctrl_readw(struct psx_ctrl *ctrl, address_t address)
{
	/* Call main read handler */
	return psx_ctrl_readl(ctrl, address);
}

uint32_t psx_ctrl_readl(struct psx_ctrl *ctrl, address_t address)
{
	union joy_rx_data joy_rx_data;
	uint8_t data = 0xFF;
	uint32_t l = 0;

	/* Handle register read */
	switch (address) {
	case JOY_RX_DATA:
		/* A data byte can be read when RX FIFO is not empty. Data
		should be read only via 8bit memory access (the 16bit/32bit
		"preview" feature is rather unusable, and usually there
		shouldn't be more than 1 byte in the FIFO anyways). */
		fifo_dequeue(&ctrl->rx_fifo, &data);
		joy_rx_data.data = data;
		joy_rx_data.preview_1 = 0;
		joy_rx_data.preview_2 = 0;
		joy_rx_data.preview_3 = 0;
		l = joy_rx_data.raw;

		/* Update RX FIFO not empty flag */
		ctrl->joy_stat.rx_fifo_not_empty = (ctrl->rx_fifo.num != 0);
		break;
	case JOY_STAT:
		/* Read register */
		l = ctrl->joy_stat.raw;
		break;
	case JOY_MODE:
		/* Read register */
		l = ctrl->joy_mode.raw;
		break;
	case JOY_CTRL:
		/* Read register */
		l = ctrl->joy_ctrl.raw;
		break;
	case JOY_BAUD:
		/* Read register */
		l = ctrl->joy_baud;
		break;
	}

	/* Return read data */
	return l;
}

void psx_ctrl_writeb(struct psx_ctrl *ctrl, uint8_t b, address_t address)
{
	/* Call main write handler */
	psx_ctrl_writel(ctrl, b, address);
}

void psx_ctrl_writew(struct psx_ctrl *ctrl, uint16_t w, address_t address)
{
	/* Call main write handler */
	psx_ctrl_writel(ctrl, w, address);
}

void psx_ctrl_writel(struct psx_ctrl *ctrl, uint32_t l, address_t address)
{
	union joy_tx_data joy_tx_data;

	/* Handle register write */
	switch (address) {
	case JOY_TX_DATA:
		/* Enqueue byte */
		joy_tx_data.data = l;
		fifo_enqueue(&ctrl->tx_fifo, joy_tx_data.data);

		/* Start the transfer (if, or as soon as TX enable = 1 and TX
		ready flag 2 = 1). The transfer starts if the current TXEN value
		or the latched TXEN value is set (ie. if TXEN gets cleared after
		writing to JOY_TX_DATA, then the transfer may STILL start if the
		old latched TXEN value was set). */
		if ((ctrl->joy_ctrl.tx_en || ctrl->tx_en_latch) &&
			ctrl->joy_stat.tx_ready_2)
			ctrl->start = true;

		/* When writing to JOY_TX_DATA, TX ready flags become zero. */
		ctrl->joy_stat.tx_ready_1 = 0;
		ctrl->joy_stat.tx_ready_2 = 0;

		/* Writing to JOY_TX_DATA latches the current TXEN value. */
		ctrl->tx_en_latch = ctrl->joy_ctrl.tx_en;
		break;
	case JOY_MODE:
		/* Write register */
		ctrl->joy_mode.raw = l;
		break;
	case JOY_CTRL:
		/* Write register */
		ctrl->joy_ctrl.raw = l;

		/* Update tramsfer start flag - it gets set if TX enable is set,
		TX ready flag 2 is set, and TX FIFO is not empty. */
		if (ctrl->joy_ctrl.tx_en && ctrl->joy_stat.tx_ready_2 &&
			(ctrl->tx_fifo.num > 0))
			ctrl->start = true;

		/* Handle acknowledge is requested */
		if (ctrl->joy_ctrl.acknowledge) {
			ctrl->joy_stat.rx_parity_error = 0;
			ctrl->joy_stat.int_req = 0;
		}

		/* Handle reset is requested and reset registers */
		if (ctrl->joy_ctrl.reset) {
			ctrl->joy_stat.raw = 0;
			ctrl->joy_stat.tx_ready_1 = 1;
			ctrl->joy_stat.tx_ready_2 = 1;
			ctrl->joy_mode.raw = 0;
			ctrl->joy_ctrl.raw = 0;
			ctrl->joy_baud = DEFAULT_BAUD;
		}

		/* Lower write-only and unused bits (always 0 when read) */
		ctrl->joy_ctrl.acknowledge = 0;
		ctrl->joy_ctrl.reset = 0;
		ctrl->joy_ctrl.unused_1 = 0;
		ctrl->joy_ctrl.unused_2 = 0;
		break;
	case JOY_BAUD:
		/* Write register and reload baudrate timer */
		ctrl->joy_baud = l;
		psx_ctrl_reload_timer(ctrl);
		break;
	}
}

bool fifo_enqueue(struct fifo *fifo, uint8_t data)
{
	/* Return if FIFO is full */
	if (fifo->num == fifo->size)
		return false;

	/* Enqueue element */
	fifo->data[fifo->pos++] = data;
	if (fifo->pos == fifo->size)
		fifo->pos = 0;

	/* Increment number of elements */
	fifo->num++;

	/* Return success */
	return true;
}

bool fifo_dequeue(struct fifo *fifo, uint8_t *data)
{
	int index;

	/* Return if FIFO is empty */
	if (fifo->num == 0)
		return false;

	/* Dequeue element */
	index = ((fifo->pos - fifo->num) + fifo->size) % fifo->size;
	*data = fifo->data[index];

	/* Decrement number of elements */
	fifo->num--;

	/* Return success */
	return true;
}

void psx_ctrl_handle_interrupts(struct psx_ctrl *ctrl)
{
	int n;

	/* Reset interrupt request bit */
	ctrl->joy_stat.int_req = 0;

	/* Check if ACK interrupt is enabled */
	if (ctrl->joy_ctrl.ack_int_en) {
		/* Interrupt when /ACK is low */
		ctrl->joy_stat.int_req |= ctrl->joy_stat.ack_input_lvl;
	}

	/* Check if TX interrupt is enabled */
	if (ctrl->joy_ctrl.tx_int_en) {
		/* Interrupt when TX ready flag 1 or 2 is set */
		ctrl->joy_stat.int_req |= ctrl->joy_stat.tx_ready_1;
		ctrl->joy_stat.int_req |= ctrl->joy_stat.tx_ready_2;
	}

	/* Check if RX interrupt is enabled */
	if (ctrl->joy_ctrl.rx_int_en) {
		/* Get RX FIFO threshold (0..3 = 1, 2, 4, 8 bytes) */
		n = 1 << ctrl->joy_ctrl.rx_int_mode;

		/* Interrupt when n bytes are in RX FIFO */
		ctrl->joy_stat.int_req |= (n == ctrl->rx_fifo.num);
	}

	/* Interrupt CPU if needed */
	if (ctrl->joy_stat.int_req)
		cpu_interrupt(ctrl->irq);
}

void psx_ctrl_reload_timer(struct psx_ctrl *ctrl)
{
	int br_factor;

	/* Get baudrate factor */
	switch (ctrl->joy_mode.br_reload_factor) {
	case MUL1A:
	case MUL1B:
		br_factor = 1;
		break;
	case MUL16:
		br_factor = 16;
		break;
	case MUL64:
		br_factor = 64;
		break;
	}

	/* Timer reload occurs when writing to JOY_BAUD, and, automatically when
	the baudrate timer reaches zero. Upon reload, the 16bit reload value is
	multiplied by the baudrate factor, divided by 2, and then copied to the
	21bit baudrate timer. */
	ctrl->joy_stat.br_timer = ctrl->joy_baud * br_factor / 2;
}

void psx_ctrl_transfer(struct psx_ctrl *ctrl)
{
	struct psx_peripheral *peripheral;
	struct list_link *link = ctrl->peripherals;
	uint8_t rx_buffer = 0xFF;
	int port;

	/* Check if new transfer has been requested */
	if (ctrl->start) {
		/* Dequeue FIFO */
		fifo_dequeue(&ctrl->tx_fifo, &ctrl->tx_buffer);

		/* As soon as the transfer starts, TX ready flag 1 becomes set
		(indicating that one can write a new byte to JOY_TX_DATA;
		although the transmission is still busy). */
		ctrl->joy_stat.tx_ready_1 = 1;

		/* Reset start flag */
		ctrl->start = false;

		/* Initialize number of bits to character length */
		switch (ctrl->joy_mode.char_len) {
		case CHAR_LEN_5BITS:
			ctrl->bit_num = 5;
			break;
		case CHAR_LEN_6BITS:
			ctrl->bit_num = 6;
			break;
		case CHAR_LEN_7BITS:
			ctrl->bit_num = 7;
			break;
		case CHAR_LEN_8BITS:
			ctrl->bit_num = 8;
			break;
		}

		/* Add parity if enabled */
		if (ctrl->joy_mode.parity_en)
			ctrl->bit_num++;

		/* Always add stop bit */
		ctrl->bit_num++;
	} else {
		/* Flag peripheral acknowledgment */
		ctrl->joy_stat.ack_input_lvl = 0;
	}

	/* Return already if previous transfer is complete */
	if (ctrl->bit_num == 0)
		return;

	/* Decrement number of bits, returning if not ready to send/receive */
	if (--ctrl->bit_num > 0)
		return;

	/* Get port (0 = JOY1, 1 = JOY2) */
	port = (ctrl->joy_ctrl.desired_slot_num == 0) ? 1 : 2;

	/* Cycle through peripherals */
	while ((peripheral = list_get_next(&link))) {
		/* Skip port if port does not match */
		if (peripheral->port != port)
			continue;

		/* Transfer data if /JOYn is low */
		if (ctrl->joy_ctrl.joyn_output)
			peripheral->receive(peripheral, ctrl->tx_buffer);

		/* RX enable should be usually zero (the hardware automatically
		enables receive when /JOYn is low). When RX enable is set, the
		next transfer causes data to be stored in RX FIFO even when
		/JOYn is high. */
		if (ctrl->joy_ctrl.rx_en || ctrl->joy_ctrl.joyn_output) {
			/* Get data from peripheral, skipping if unsuccessful */
			if (!peripheral->send(peripheral, &rx_buffer))
				continue;

			/* Flag peripheral acknowledgment */
			ctrl->joy_stat.ack_input_lvl = 1;
		}
	}

	/* Enqueue data in RX FIFO and update empty flag */
	fifo_enqueue(&ctrl->rx_fifo, rx_buffer);
	ctrl->joy_stat.rx_fifo_not_empty = 1;

	/* As soon as the transfer of the most recently written byte ends, TX
	ready flag 2 becomes set. */
	ctrl->joy_stat.tx_ready_2 = 1;

	/* The hardware automatically clears RX enable after the transfer. */
	ctrl->joy_ctrl.rx_en = 0;
}

void psx_ctrl_tick(struct psx_ctrl *ctrl)
{
	/* The 21bit timer decreases at 33MHz. */
	ctrl->joy_stat.br_timer--;
	clock_consume(1);

	/* Return already if timer is not 0 */
	if (ctrl->joy_stat.br_timer != 0)
		return;

	/* Reload baudrate timer */
	psx_ctrl_reload_timer(ctrl);

	/* Update bit clock state and return if bit clock is high (the timer
	elapses twice per bit) */
	ctrl->bit_clk_state = !ctrl->bit_clk_state;
	if (ctrl->bit_clk_state)
		return;

	/* Handle transfer */
	psx_ctrl_transfer(ctrl);

	/* Check for interrupts and handle them */
	psx_ctrl_handle_interrupts(ctrl);
}

bool psx_ctrl_init(struct controller_instance *instance)
{
	struct psx_ctrl *ctrl;
	struct resource *res;

	/* Allocate controller structure */
	instance->priv_data = calloc(1, sizeof(struct psx_ctrl));
	ctrl = instance->priv_data;

	/* Add controller memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	ctrl->region.area = res;
	ctrl->region.mops = &psx_ctrl_mops;
	ctrl->region.data = ctrl;
	memory_region_add(&ctrl->region);

	/* Add clock */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	ctrl->clock.rate = res->data.clk;
	ctrl->clock.data = ctrl;
	ctrl->clock.tick = (clock_tick_t)psx_ctrl_tick;
	clock_add(&ctrl->clock);

	/* Get IRQ */
	res = resource_get("irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	ctrl->irq = res->data.irq;

	/* Allocate FIFOs */
	ctrl->tx_fifo.data = calloc(TX_FIFO_SIZE, sizeof(uint8_t));
	ctrl->tx_fifo.size = TX_FIFO_SIZE;
	ctrl->rx_fifo.data = calloc(RX_FIFO_SIZE, sizeof(uint8_t));
	ctrl->rx_fifo.size = RX_FIFO_SIZE;

	return true;
}

void psx_ctrl_reset(struct controller_instance *instance)
{
	struct psx_ctrl *ctrl = instance->priv_data;

	/* Reset registers and states */
	ctrl->joy_stat.raw = 0;
	ctrl->joy_stat.tx_ready_1 = 1;
	ctrl->joy_stat.tx_ready_2 = 1;
	ctrl->joy_mode.raw = 0;
	ctrl->joy_ctrl.raw = 0;
	ctrl->joy_baud = DEFAULT_BAUD;
	ctrl->tx_buffer = 0;
	ctrl->start = false;
	ctrl->tx_en_latch = false;
	ctrl->bit_clk_state = false;
	ctrl->bit_num = 0;

	/* Empty FIFOs */
	ctrl->tx_fifo.pos = 0;
	ctrl->tx_fifo.num = 0;
	ctrl->rx_fifo.pos = 0;
	ctrl->rx_fifo.num = 0;

	/* Enable clock */
	ctrl->clock.enabled = true;
}

void psx_ctrl_deinit(struct controller_instance *instance)
{
	struct psx_ctrl *ctrl = instance->priv_data;
	list_remove_all(&ctrl->peripherals);
	free(ctrl->tx_fifo.data);
	free(ctrl->rx_fifo.data);
	free(ctrl);
}

void psx_ctrl_add(struct controller_instance *i, struct psx_peripheral *p)
{
	struct psx_ctrl *ctrl = i->priv_data;

	/* Add peripheral to list */
	list_insert(&ctrl->peripherals, p);
}

CONTROLLER_START(psx_controller)
	.init = psx_ctrl_init,
	.reset = psx_ctrl_reset,
	.deinit = psx_ctrl_deinit
CONTROLLER_END

