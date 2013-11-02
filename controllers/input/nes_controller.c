#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <bitops.h>
#include <clock.h>
#include <controller.h>
#include <input.h>
#include <memory.h>
#include <resource.h>
#include <util.h>

#define INPUT		0
#define NUM_PLAYERS	2
#define NUM_KEYS	8

union input_reg {
	uint8_t value;
	struct {
		uint8_t strobe:1;
		uint8_t reserved:7;
	};
};

union output_reg {
	uint8_t value;
	struct {
		uint8_t serial_data:1;
		uint8_t reserved:4;
		uint8_t open_bus:3;
	};
};

struct nes_ctrl {
	union input_reg input_reg;
	uint8_t shift_regs[NUM_PLAYERS];
	struct input_config input_config;
	int bus_id;
	bool keys[NUM_PLAYERS][NUM_KEYS];
};

static bool nes_ctrl_init(struct controller_instance *instance);
static void nes_ctrl_deinit(struct controller_instance *instance);
static void nes_ctrl_event(int id, struct input_state *s, input_data_t *d);
static uint8_t nes_ctrl_readb(region_data_t *data, address_t address);
static void nes_ctrl_writeb(region_data_t *data, uint8_t b, address_t address);
static void nes_ctrl_reload(struct nes_ctrl *nes_ctrl);

static struct input_event default_input_events[] = {
	{ EVENT_KEYBOARD, { { 'q' } } },	/* Player 1 - A */
	{ EVENT_KEYBOARD, { { 'w' } } },	/* Player 1 - B */
	{ EVENT_KEYBOARD, { { 'o' } } },	/* Player 1 - Select */
	{ EVENT_KEYBOARD, { { 'p' } } },	/* Player 1 - Start */
	{ EVENT_KEYBOARD, { { 'i' } } },	/* Player 1 - Up */
	{ EVENT_KEYBOARD, { { 'k' } } },	/* Player 1 - Down */
	{ EVENT_KEYBOARD, { { 'j' } } },	/* Player 1 - Left */
	{ EVENT_KEYBOARD, { { 'l' } } },	/* Player 1 - Right */
	{ EVENT_KEYBOARD, { { 'e' } } },	/* Player 2 - A */
	{ EVENT_KEYBOARD, { { 'r' } } },	/* Player 2 - B */
	{ EVENT_KEYBOARD, { { 'n' } } },	/* Player 2 - Select */
	{ EVENT_KEYBOARD, { { 'm' } } },	/* Player 2 - Start */
	{ EVENT_KEYBOARD, { { 'y' } } },	/* Player 2 - Up */
	{ EVENT_KEYBOARD, { { 'h' } } },	/* Player 2 - Down */
	{ EVENT_KEYBOARD, { { 'g' } } },	/* Player 2 - Left */
	{ EVENT_KEYBOARD, { { 'j' } } }		/* Player 2 - Right */
};

static struct mops nes_ctrl_mops = {
	.readb = nes_ctrl_readb,
	.writeb = nes_ctrl_writeb
};

uint8_t nes_ctrl_readb(region_data_t *data, address_t address)
{
	struct nes_ctrl *nes_ctrl = data;
	union output_reg output_reg;

	/* Initialize output (faking open bus contents) */
	output_reg.reserved = 0;
	output_reg.open_bus = 0x07;

	/* Load serial data from shift register */
	output_reg.serial_data = nes_ctrl->shift_regs[address] & BIT(0);

	/* Shift register if needed */
	if (!nes_ctrl->input_reg.strobe)
		nes_ctrl->shift_regs[address] >>= 1;

	return output_reg.value;
}

void nes_ctrl_writeb(region_data_t *data, uint8_t b, address_t address)
{
	struct nes_ctrl *nes_ctrl = data;

	/* Store data in input register */
	if (address == INPUT) {
		nes_ctrl->input_reg.value = b;

		/* Reload shift register if needed */
		if (nes_ctrl->input_reg.strobe)
			nes_ctrl_reload(nes_ctrl);
	}
}

void nes_ctrl_reload(struct nes_ctrl *nes_ctrl)
{
	int i;
	int j;

	/* Fill shift registers */
	for (i = 0; i < NUM_PLAYERS; i++) {
		/* Reset data */
		nes_ctrl->shift_regs[i] = 0;

		/* Fill data */
		for (j = 0; j < NUM_KEYS; j++)
			nes_ctrl->shift_regs[i] |= nes_ctrl->keys[i][j] << j;
	}
}

bool nes_ctrl_init(struct controller_instance *instance)
{
	struct nes_ctrl *nes_ctrl;
	struct resource *area;
	struct input_config *input_config;
	char *name;
	int num_events;
	int i;

	/* Allocate nes_ctrl structure */
	instance->priv_data = malloc(sizeof(struct nes_ctrl));
	nes_ctrl = instance->priv_data;

	/* Set up nes_ctrl memory region */
	area = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	memory_region_add(area, &nes_ctrl_mops, instance->priv_data);

	/* Save bus ID for later use */
	nes_ctrl->bus_id = instance->bus_id;

	/* Initialize controller data */
	nes_ctrl->input_reg.strobe = 0;
	for (i = 0; i < NUM_PLAYERS; i++)
		memset(nes_ctrl->keys[i], 0, NUM_KEYS * sizeof(bool));

	/* Set number of events to handle (for both players) */
	num_events = NUM_PLAYERS * NUM_KEYS;

	/* Initialize input configuration */
	input_config = &nes_ctrl->input_config;
	input_config->events = malloc(num_events * sizeof(struct input_event));
	input_config->num_events = num_events;
	input_config->callback = nes_ctrl_event;
	input_config->data = nes_ctrl;

	/* Load and register input config (fall back to defaults if needed) */
	name = instance->controller->name;
	if (!input_load(name, input_config->events, num_events))
		memcpy(input_config->events,
			default_input_events,
			num_events * sizeof(struct input_event));
	input_register(input_config);

	return true;
}

void nes_ctrl_event(int id, struct input_state *state, input_data_t *data)
{
	struct nes_ctrl *nes_ctrl = data;
	int key;
	int player;

	/* Determine key and player */
	key = id % NUM_KEYS;
	player = id / NUM_KEYS;

	/* Save key state */
	nes_ctrl->keys[player][key] = state->active;

	/* Reload shift register if needed */
	if (nes_ctrl->input_reg.strobe)
		nes_ctrl_reload(nes_ctrl);
}

void nes_ctrl_deinit(struct controller_instance *instance)
{
	struct nes_ctrl *nes_ctrl = instance->priv_data;
	input_unregister(&nes_ctrl->input_config);
	free(nes_ctrl->input_config.events);
	free(nes_ctrl);
}

CONTROLLER_START(nes_controller)
	.init = nes_ctrl_init,
	.deinit = nes_ctrl_deinit
CONTROLLER_END

