#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <bitops.h>
#include <clock.h>
#include <controller.h>
#include <input.h>
#ifdef __LIBRETRO__
#include <libretro.h>
#endif
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
	bool keys[NUM_PLAYERS][NUM_KEYS];
	struct region region;
};

static bool nes_ctrl_init(struct controller_instance *instance);
static void nes_ctrl_reset(struct controller_instance *instance);
static void nes_ctrl_deinit(struct controller_instance *instance);
static void nes_ctrl_event(int id, enum input_type type, struct nes_ctrl *n);
static uint8_t nes_ctrl_readb(struct nes_ctrl *nes_ctrl, address_t a);
static void nes_ctrl_writeb(struct nes_ctrl *nes_ctrl, uint8_t b, address_t a);
static void nes_ctrl_reload(struct nes_ctrl *nes_ctrl);

static struct input_desc input_descs[] = {
#ifndef __LIBRETRO__
	{ "Player 1 A", DEVICE_KEYBOARD, KEY_q },
	{ "Player 1 B", DEVICE_KEYBOARD, KEY_w },
	{ "Player 1 Select", DEVICE_KEYBOARD, KEY_o },
	{ "Player 1 Start", DEVICE_KEYBOARD, KEY_p },
	{ "Player 1 Up", DEVICE_KEYBOARD, KEY_UP },
	{ "Player 1 Down", DEVICE_KEYBOARD, KEY_DOWN },
	{ "Player 1 Left", DEVICE_KEYBOARD, KEY_LEFT },
	{ "Player 1 Right", DEVICE_KEYBOARD, KEY_RIGHT },
	{ "Player 2 A", DEVICE_KEYBOARD, KEY_e },
	{ "Player 2 B", DEVICE_KEYBOARD, KEY_r },
	{ "Player 2 Select", DEVICE_KEYBOARD, KEY_n },
	{ "Player 2 Start", DEVICE_KEYBOARD, KEY_m },
	{ "Player 2 Up", DEVICE_KEYBOARD, KEY_i },
	{ "Player 2 Down", DEVICE_KEYBOARD, KEY_k },
	{ "Player 2 Left", DEVICE_KEYBOARD, KEY_j },
	{ "Player 2 Right", DEVICE_KEYBOARD, KEY_l }
#else
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_A },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_B },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_SELECT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_START },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_UP },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_DOWN },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_LEFT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_RIGHT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(1) | RETRO_DEVICE_ID_JOYPAD_A },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(1) | RETRO_DEVICE_ID_JOYPAD_B },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(1) | RETRO_DEVICE_ID_JOYPAD_SELECT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(1) | RETRO_DEVICE_ID_JOYPAD_START },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(1) | RETRO_DEVICE_ID_JOYPAD_UP },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(1) | RETRO_DEVICE_ID_JOYPAD_DOWN },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(1) | RETRO_DEVICE_ID_JOYPAD_LEFT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(1) | RETRO_DEVICE_ID_JOYPAD_RIGHT }
#endif
};

static struct mops nes_ctrl_mops = {
	.readb = (readb_t)nes_ctrl_readb,
	.writeb = (writeb_t)nes_ctrl_writeb
};

uint8_t nes_ctrl_readb(struct nes_ctrl *nes_ctrl, address_t address)
{
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

void nes_ctrl_writeb(struct nes_ctrl *nes_ctrl, uint8_t b, address_t address)
{
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

	/* Allocate nes_ctrl structure */
	instance->priv_data = malloc(sizeof(struct nes_ctrl));
	nes_ctrl = instance->priv_data;

	/* Set up nes_ctrl memory region */
	area = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	nes_ctrl->region.area = area;
	nes_ctrl->region.mops = &nes_ctrl_mops;
	nes_ctrl->region.data = nes_ctrl;
	memory_region_add(&nes_ctrl->region);

	/* Initialize input configuration */
	input_config = &nes_ctrl->input_config;
	input_config->name = instance->controller_name;
	input_config->descs = input_descs;
	input_config->num_descs = ARRAY_SIZE(input_descs);
	input_config->data = nes_ctrl;
	input_config->callback = (input_cb_t)nes_ctrl_event;
	input_register(input_config, true);

	return true;
}

void nes_ctrl_reset(struct controller_instance *instance)
{
	struct nes_ctrl *nes_ctrl = instance->priv_data;
	int i;

	/* Initialize controller data */
	nes_ctrl->input_reg.strobe = 0;
	for (i = 0; i < NUM_PLAYERS; i++)
		memset(nes_ctrl->keys[i], 0, NUM_KEYS * sizeof(bool));
}

void nes_ctrl_event(int id, enum input_type type, struct nes_ctrl *nes_ctrl)
{
	int key;
	int player;

	/* Determine key and player */
	key = id % NUM_KEYS;
	player = id / NUM_KEYS;

	/* Save key state */
	nes_ctrl->keys[player][key] = (type == EVENT_BUTTON_DOWN);

	/* Reload shift register if needed */
	if (nes_ctrl->input_reg.strobe)
		nes_ctrl_reload(nes_ctrl);
}

void nes_ctrl_deinit(struct controller_instance *instance)
{
	struct nes_ctrl *nes_ctrl = instance->priv_data;
	input_unregister(&nes_ctrl->input_config);
	free(nes_ctrl);
}

CONTROLLER_START(nes_controller)
	.init = nes_ctrl_init,
	.reset = nes_ctrl_reset,
	.deinit = nes_ctrl_deinit
CONTROLLER_END

