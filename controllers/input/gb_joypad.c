#include <stdlib.h>
#include <controller.h>
#include <memory.h>
#include <util.h>

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
};

static bool joypad_init(struct controller_instance *instance);
static void joypad_deinit(struct controller_instance *instance);
static uint8_t joypad_readb(union joypad_reg *reg, address_t address);
static void joypad_writeb(union joypad_reg *reg, uint8_t b, address_t address);

static struct mops joypad_mops = {
	.readb = (readb_t)joypad_readb,
	.writeb = (writeb_t)joypad_writeb
};

uint8_t joypad_readb(union joypad_reg *reg, uint16_t UNUSED(address))
{
	union joypad_reg output;

	/* Return value from register with select bits set */
	output.value = reg->value;
	output.select_direction_keys = 1;
	output.select_button_keys = 1;
	return output.value;
}

void joypad_writeb(union joypad_reg *reg, uint8_t b, uint16_t UNUSED(address))
{
	union joypad_reg input;

	/* Save select bits only */
	input.value = b;
	reg->select_direction_keys = input.select_direction_keys;
	reg->select_button_keys = input.select_button_keys;
}

bool joypad_init(struct controller_instance *instance)
{
	struct joypad *joypad;
	struct resource *area;

	/* Allocate joypad structure */
	instance->priv_data = malloc(sizeof(struct joypad));
	joypad = instance->priv_data;

	/* Set up joypad memory region */
	area = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	joypad->region.area = area;
	joypad->region.mops = &joypad_mops;
	joypad->region.data = &joypad->reg;
	memory_region_add(&joypad->region);

	/* Initialize joypad register */
	joypad->reg.input_right_or_a = 1;
	joypad->reg.input_left_or_b = 1;
	joypad->reg.input_up_or_select = 1;
	joypad->reg.input_down_or_start = 1;
	joypad->reg.select_direction_keys = 1;
	joypad->reg.select_button_keys = 1;
	joypad->reg.reserved = 1;

	return true;
}

void joypad_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(gb_joypad)
	.init = joypad_init,
	.deinit = joypad_deinit
CONTROLLER_END

