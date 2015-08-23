#include <stdio.h>
#include <stdbool.h>
#include <caca.h>
#include <input.h>
#include <util.h>

static bool caca_init(struct input_frontend *fe, window_t *window);
static void caca_update(struct input_frontend *fe);
static void key_event(caca_event_t *e);
static void mouse_event(caca_event_t *e);
static void quit_event();

void key_event(caca_event_t *e)
{
	struct input_event event;
	bool down;

	/* Fill event information (caca codes match input system codes) */
	down = (e->type == CACA_EVENT_KEY_PRESS);
	event.device = DEVICE_KEYBOARD;
	event.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;

	/* Fill event code (left and right arrows do not match by default) */
	switch (caca_get_event_key_ch(e)) {
	case CACA_KEY_LEFT:
		event.code = KEY_LEFT;
		break;
	case CACA_KEY_RIGHT:
		event.code = KEY_RIGHT;
		break;
	default:
		event.code = caca_get_event_key_ch(e);
		break;
	}

	/* Report event */
	input_report(&event);
}

void mouse_event(caca_event_t *e)
{
	struct input_event event;
	bool down;

	/* Fill event information (caca codes match input system codes) */
	down = (e->type == CACA_EVENT_MOUSE_PRESS);
	event.device = DEVICE_MOUSE;
	event.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;
	event.code = caca_get_event_mouse_button(e);

	/* Report event */
	input_report(&event);
}

void quit_event()
{
	struct input_event event;

	/* Fill event information */
	event.device = DEVICE_NONE;
	event.type = EVENT_QUIT;
	event.code = GENERIC_QUIT;

	/* Report event */
	input_report(&event);
}

bool caca_init(struct input_frontend *fe, window_t *window)
{
	/* Save window */
	fe->priv_data = window;

	return true;
}

void caca_update(struct input_frontend *fe)
{
	caca_display_t *dp = fe->priv_data;
	caca_event_t e;

	/* Poll all events out of queue */
	while (caca_get_event(dp, CACA_EVENT_ANY, &e, 0)) {
		switch (e.type) {
		case CACA_EVENT_KEY_PRESS:
		case CACA_EVENT_KEY_RELEASE:
			key_event(&e);
			break;
		case CACA_EVENT_MOUSE_PRESS:
		case CACA_EVENT_MOUSE_RELEASE:
			mouse_event(&e);
			break;
		case CACA_EVENT_QUIT:
			quit_event();
			break;
		default:
			break;
		}
	}
}

INPUT_START(caca)
	.init = caca_init,
	.update = caca_update
INPUT_END

