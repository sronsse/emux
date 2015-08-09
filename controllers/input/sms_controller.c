#include <stdlib.h>
#include <bitops.h>
#include <controller.h>
#include <input.h>
#ifdef __LIBRETRO__
#include <libretro.h>
#endif
#include <port.h>
#include <util.h>

union ctl_port {
	uint8_t raw;
	struct {
		uint8_t p_a_tr_dir:1;
		uint8_t p_a_th_dir:1;
		uint8_t p_b_tr_dir:1;
		uint8_t p_b_th_dir:1;
		uint8_t p_a_tr_lvl:1;
		uint8_t p_a_th_lvl:1;
		uint8_t p_b_tr_lvl:1;
		uint8_t p_b_th_lvl:1;
	};
};

union ab_port {
	uint8_t raw;
	struct {
		uint8_t p_a_up:1;
		uint8_t p_a_down:1;
		uint8_t p_a_left:1;
		uint8_t p_a_right:1;
		uint8_t p_a_tl:1;
		uint8_t p_a_tr:1;
		uint8_t p_b_up:1;
		uint8_t p_b_down:1;
	};
};

struct sms_ctrl {
	union ctl_port ctl_port;
	union ab_port ab_port;
	struct port_region ctl_region;
	struct port_region io_region;
	struct input_config input_config;
};

static bool sms_ctrl_init(struct controller_instance *instance);
static void sms_ctrl_reset(struct controller_instance *instance);
static void sms_ctrl_deinit(struct controller_instance *instance);
static void sms_ctrl_event(int id, enum input_type type, struct sms_ctrl *s);
static void ctl_write(struct sms_ctrl *sms_ctrl, uint8_t b, port_t port);
static uint8_t io_read(struct sms_ctrl *sms_ctrl, port_t port);

static struct input_desc input_descs[] = {
#ifndef __LIBRETRO__
	{ "Up", DEVICE_KEYBOARD, KEY_UP },
	{ "Down", DEVICE_KEYBOARD, KEY_DOWN },
	{ "Left", DEVICE_KEYBOARD, KEY_LEFT },
	{ "Right", DEVICE_KEYBOARD, KEY_RIGHT },
	{ "A", DEVICE_KEYBOARD, KEY_q },
	{ "B", DEVICE_KEYBOARD, KEY_w },
	{ "Up", DEVICE_KEYBOARD, KEY_i },
	{ "Down", DEVICE_KEYBOARD, KEY_k }
#else
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_UP },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_DOWN },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_LEFT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_RIGHT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_A },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_B },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(1) | RETRO_DEVICE_ID_JOYPAD_UP },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(1) | RETRO_DEVICE_ID_JOYPAD_DOWN }
#endif
};

static struct pops ctl_pops = {
	.write = (write_t)ctl_write
};

static struct pops io_pops = {
	.read = (read_t)io_read
};

void ctl_write(struct sms_ctrl *sms_ctrl, uint8_t b, port_t port)
{
	/* Handle odd addresses only */
	if (!(port & 0x01))
		return;

	/* Save control port register */
	sms_ctrl->ctl_port.raw = b;
}

uint8_t io_read(struct sms_ctrl *sms_ctrl, port_t port)
{
	/* Return A/B state for even addresses */
	if (!(port & 0x01))
		return sms_ctrl->ab_port.raw;

	/* Return invalid data otherwise */
	return 0xFF;
}

void sms_ctrl_event(int id, enum input_type type, struct sms_ctrl *sms_ctrl)
{
	/* Set corresponding bit in port state */
	bitops_setb(&sms_ctrl->ab_port.raw,
		id,
		1,
		(type != EVENT_BUTTON_DOWN));
}

bool sms_ctrl_init(struct controller_instance *instance)
{
	struct sms_ctrl *sms_ctrl;
	struct input_config *input_config;
	struct resource *area;

	/* Allocate sms_ctrl structure */
	instance->priv_data = malloc(sizeof(struct sms_ctrl));
	sms_ctrl = instance->priv_data;

	/* Set up control port region */
	area = resource_get("ctl",
		RESOURCE_PORT,
		instance->resources,
		instance->num_resources);
	sms_ctrl->ctl_region.area = area;
	sms_ctrl->ctl_region.pops = &ctl_pops;
	sms_ctrl->ctl_region.data = sms_ctrl;
	port_region_add(&sms_ctrl->ctl_region);

	/* Set up I/O port region */
	area = resource_get("io",
		RESOURCE_PORT,
		instance->resources,
		instance->num_resources);
	sms_ctrl->io_region.area = area;
	sms_ctrl->io_region.pops = &io_pops;
	sms_ctrl->io_region.data = sms_ctrl;
	port_region_add(&sms_ctrl->io_region);

	/* Initialize input configuration */
	input_config = &sms_ctrl->input_config;
	input_config->name = instance->controller_name;
	input_config->descs = input_descs;
	input_config->num_descs = ARRAY_SIZE(input_descs);
	input_config->data = sms_ctrl;
	input_config->callback = (input_cb_t)sms_ctrl_event;
	input_register(input_config, true);

	return true;
}

void sms_ctrl_reset(struct controller_instance *instance)
{
	struct sms_ctrl *sms_ctrl = instance->priv_data;

	/* Set control lines as input (1 = input) */
	sms_ctrl->ctl_port.p_a_tr_dir = 1;
	sms_ctrl->ctl_port.p_a_th_dir = 1;
	sms_ctrl->ctl_port.p_b_tr_dir = 1;
	sms_ctrl->ctl_port.p_b_tr_dir = 1;

	/* Initialize A/B state (1 = not pressed) */
	sms_ctrl->ab_port.p_a_up = 1;
	sms_ctrl->ab_port.p_a_down = 1;
	sms_ctrl->ab_port.p_a_left = 1;
	sms_ctrl->ab_port.p_a_right = 1;
	sms_ctrl->ab_port.p_a_tl = 1;
	sms_ctrl->ab_port.p_a_tr = 1;
	sms_ctrl->ab_port.p_b_up = 1;
	sms_ctrl->ab_port.p_b_down = 1;
}

void sms_ctrl_deinit(struct controller_instance *instance)
{
	struct sms_ctrl *sms_ctrl = instance->priv_data;
	free(sms_ctrl);
}

CONTROLLER_START(sms_controller)
	.init = sms_ctrl_init,
	.reset = sms_ctrl_reset,
	.deinit = sms_ctrl_deinit
CONTROLLER_END

