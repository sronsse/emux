#include <stdlib.h>
#include <controller.h>
#include <cpu.h>
#include <input.h>
#ifdef __LIBRETRO__
#include <libretro.h>
#endif
#include <memory.h>
#include <util.h>

#define NUM_KEYS	8
#define BUTTON_OFFSET	0
#define DIR_OFFSET	4

union joypad_reg {
	uint8_t value;
	struct {
		uint8_t input_right_or_a:1;
		uint8_t input_left_or_b:1;
		uint8_t input_up_or_select:1;
		uint8_t input_down_or_start:1;
		uint8_t select_direction_keys:1;
		uint8_t select_button_keys:1;
		uint8_t reserved:2;
	};
};

struct joypad {
	union joypad_reg reg;
	struct region region;
	int irq;
	struct input_config input_config;
	bool keys[NUM_KEYS];
};

static bool joypad_init(struct controller_instance *instance);
static void joypad_reset(struct controller_instance *instance);
static void joypad_deinit(struct controller_instance *instance);
static uint8_t joypad_readb(struct joypad *joypad, address_t address);
static void joypad_writeb(struct joypad *joypad, uint8_t b, address_t address);
static void joypad_event(int id, enum input_type type, struct joypad *joypad);
static void update_reg(struct joypad *joypad);

static struct input_desc input_descs[] = {
#ifndef __LIBRETRO__
	{ "A", DEVICE_KEYBOARD, KEY_q },
	{ "B", DEVICE_KEYBOARD, KEY_w },
	{ "Select", DEVICE_KEYBOARD, KEY_o },
	{ "Start", DEVICE_KEYBOARD, KEY_p },
	{ "Right", DEVICE_KEYBOARD, KEY_RIGHT },
	{ "Left", DEVICE_KEYBOARD, KEY_LEFT },
	{ "Up", DEVICE_KEYBOARD, KEY_UP },
	{ "Down", DEVICE_KEYBOARD, KEY_DOWN }
#else
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_A },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_B },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_SELECT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_START },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_RIGHT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_LEFT },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_UP },
	{ NULL, RETRO_DEVICE_JOYPAD, PRT(0) | RETRO_DEVICE_ID_JOYPAD_DOWN }
#endif
};

static struct mops joypad_mops = {
	.readb = (readb_t)joypad_readb,
	.writeb = (writeb_t)joypad_writeb
};

void update_reg(struct joypad *joypad)
{
	int offset;

	/* Set offset based on button/direction selection */
	offset = !joypad->reg.select_button_keys ? BUTTON_OFFSET : DIR_OFFSET;

	/* Set register based on key states */
	joypad->reg.input_right_or_a = !joypad->keys[offset];
	joypad->reg.input_left_or_b = !joypad->keys[offset + 1];
	joypad->reg.input_up_or_select = !joypad->keys[offset + 2];
	joypad->reg.input_down_or_start = !joypad->keys[offset + 3];
}

uint8_t joypad_readb(struct joypad *joypad, address_t UNUSED(address))
{
	union joypad_reg output;

	/* Return value from register value with select bits set */
	output.value = joypad->reg.value;
	output.select_direction_keys = 1;
	output.select_button_keys = 1;
	return output.value;
}

void joypad_writeb(struct joypad *joypad, uint8_t b, address_t UNUSED(address))
{
	union joypad_reg input;

	/* Save select bits only and update register */
	input.value = b;
	joypad->reg.select_direction_keys = input.select_direction_keys;
	joypad->reg.select_button_keys = input.select_button_keys;
	update_reg(joypad);
}

void joypad_event(int id, enum input_type type, struct joypad *joypad)
{
	/* Save key state, update register, and interrupt CPU */
	joypad->keys[id] = (type == EVENT_BUTTON_DOWN);
	update_reg(joypad);
	cpu_interrupt(joypad->irq);
}

bool joypad_init(struct controller_instance *instance)
{
	struct joypad *joypad;
	struct input_config *input_config;
	struct resource *res;

	/* Allocate joypad structure */
	instance->priv_data = calloc(1, sizeof(struct joypad));
	joypad = instance->priv_data;

	/* Initialize input configuration */
	input_config = &joypad->input_config;
	input_config->name = instance->controller_name;
	input_config->descs = input_descs;
	input_config->num_descs = ARRAY_SIZE(input_descs);
	input_config->data = joypad;
	input_config->callback = (input_cb_t)joypad_event;
	input_register(input_config, true);

	/* Set up joypad memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	joypad->region.area = res;
	joypad->region.mops = &joypad_mops;
	joypad->region.data = joypad;
	memory_region_add(&joypad->region);

	/* Get IRQ number */
	res = resource_get("irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	joypad->irq = res->data.irq;

	return true;
}

void joypad_reset(struct controller_instance *instance)
{
	struct joypad *joypad = instance->priv_data;
	int i;

	/* Initialize joypad register */
	joypad->reg.value = 0xFF;

	/* Initialize key states */
	for (i = 0; i < NUM_KEYS; i++)
		joypad->keys[i] = false;
}

void joypad_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(gb_joypad)
	.init = joypad_init,
	.reset = joypad_reset,
	.deinit = joypad_deinit
CONTROLLER_END

