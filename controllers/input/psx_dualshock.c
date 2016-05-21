#include <clock.h>
#include <controller.h>
#include <input.h>
#ifdef __LIBRETRO__
#include <libretro.h>
#endif
#include <util.h>
#include "psx_controller.h"

struct psx_dualshock {
	uint8_t counter;
	bool active;
	uint16_t state;
	struct input_config input_config;
};

static bool psx_dualshock_init(struct controller_instance *instance);
static void psx_dualshock_reset(struct controller_instance *instance);
static void psx_dualshock_deinit(struct controller_instance *instance);
static bool psx_dualshock_send(struct psx_peripheral *p, uint8_t *data);
static void psx_dualshock_receive(struct psx_peripheral *p, uint8_t data);
static void psx_dualshock_event(int, enum input_type, struct psx_dualshock *);

static struct input_desc input_descs[] = {
#ifndef __LIBRETRO__
	{ "Select Button", DEVICE_KEYBOARD, KEY_o },
	{ "L3/Joy-button", DEVICE_KEYBOARD, KEY_u },
	{ "R3/Joy-button", DEVICE_KEYBOARD, KEY_i },
	{ "Start Button", DEVICE_KEYBOARD, KEY_p },
	{ "Joypad Up", DEVICE_KEYBOARD, KEY_UP },
	{ "Joypad Right", DEVICE_KEYBOARD, KEY_RIGHT },
	{ "Joypad Down", DEVICE_KEYBOARD, KEY_DOWN },
	{ "Joypad Left", DEVICE_KEYBOARD, KEY_LEFT },
	{ "L2 Button", DEVICE_KEYBOARD, KEY_q },
	{ "R2 Button", DEVICE_KEYBOARD, KEY_e },
	{ "L1 Button", DEVICE_KEYBOARD, KEY_z },
	{ "R1 Button", DEVICE_KEYBOARD, KEY_x },
	{ "Triangle Button", DEVICE_KEYBOARD, KEY_w },
	{ "Circle Button", DEVICE_KEYBOARD, KEY_d },
	{ "X Button", DEVICE_KEYBOARD, KEY_s },
	{ "Square Button", DEVICE_KEYBOARD, KEY_a }
#else
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_SELECT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_L3 },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_R3 },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_START },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_UP },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_RIGHT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_DOWN },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_LEFT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_L2 },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_R2 },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_L1 },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_R1 },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_X },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_A },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_B },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_Y }
#endif
};

bool psx_dualshock_send(struct psx_peripheral *peripheral, uint8_t *data)
{
	struct psx_dualshock *ds = peripheral->priv_data;

	/* Fill data based on current counter if active (and increment it) */
	if (ds->active)
		switch (ds->counter++) {
		case 0:
			/* Fill with Hi-Z (unused byte) */
			*data = 0xFF;
			break;
		case 1:
			/* Fill with idlo */
			*data = 0x41;
			break;
		case 2:
			/* Fill with idhi */
			*data = 0x5A;
			break;
		case 3:
			/* Fill with swlo */
			*data = ds->state;
			break;
		case 4:
			/* Fill with swhi and deactivate */
			*data = ds->state >> 8;
			ds->active = false;
			break;
		}

	/* Return active state (indicating if we have more data to send) */
	return ds->active;
}

void psx_dualshock_receive(struct psx_peripheral *peripheral, uint8_t data)
{
	struct psx_dualshock *ds = peripheral->priv_data;

	/* Handle received command */
	switch (data) {
	case 0x01:
		/* Activate and reset counter */
		ds->active = true;
		ds->counter = 0;
		break;
	default:
		/* Discard all other commands */
		break;
	}
}

void psx_dualshock_event(int id, enum input_type type, struct psx_dualshock *ds)
{
	/* Save button state (0 = pressed, 1 = released) */
	ds->state |= (1 << id);
	if (type == EVENT_BUTTON_DOWN)
		ds->state &= ~(1 << id);
}

bool psx_dualshock_init(struct controller_instance *instance)
{
	struct controller_instance *psx_ctrl_instance;
	struct psx_peripheral *peripheral;
	struct psx_dualshock *ds;
	struct input_config *input_config;
	struct resource *res;

	/* Allocate peripheral structure */
	instance->priv_data = calloc(1, sizeof(struct psx_peripheral));
	peripheral = instance->priv_data;

	/* Fill port */
	res = resource_get("port",
		RESOURCE_PORT,
		instance->resources,
		instance->num_resources);
	peripheral->port = res->data.port.start;

	/* Fill send/receive handlers */
	peripheral->send = psx_dualshock_send;
	peripheral->receive = psx_dualshock_receive;

	/* Get PSX controller instance from machine data and register to it */
	psx_ctrl_instance = instance->mach_data;
	psx_ctrl_add(psx_ctrl_instance, peripheral);

	/* Allocate peripheral private data */
	peripheral->priv_data = calloc(1, sizeof(struct psx_dualshock));
	ds = peripheral->priv_data;

	/* Initialize input configuration */
	input_config = &ds->input_config;
	input_config->name = instance->controller_name;
	input_config->descs = input_descs;
	input_config->num_descs = ARRAY_SIZE(input_descs);
	input_config->data = ds;
	input_config->callback = (input_cb_t)psx_dualshock_event;
	input_register(input_config, true);

	return true;
}

void psx_dualshock_reset(struct controller_instance *instance)
{
	struct psx_peripheral *peripheral = instance->priv_data;
	struct psx_dualshock *ds = peripheral->priv_data;

	/* Reset data */
	ds->counter = 0;
	ds->active = false;
	ds->state = 0xFFFF;
}

void psx_dualshock_deinit(struct controller_instance *instance)
{
	struct psx_peripheral *peripheral = instance->priv_data;
	struct psx_dualshock *ds = peripheral->priv_data;
	input_unregister(&ds->input_config);
	free(peripheral->priv_data);
	free(peripheral);
}

CONTROLLER_START(psx_dualshock)
	.init = psx_dualshock_init,
	.reset = psx_dualshock_reset,
	.deinit = psx_dualshock_deinit
CONTROLLER_END

