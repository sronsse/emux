#include <stdio.h>
#include <stdbool.h>
#include <caca.h>
#include <input.h>
#include <util.h>

static bool caca_init(struct input_frontend *fe, window_t *window);
static void caca_update(struct input_frontend *fe);
static void caca_deinit(struct input_frontend *fe);

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
	struct input_event event;
	struct input_state state;

	/* Poll all events out of queue */
	while (caca_get_event(dp, CACA_EVENT_ANY, &e, 0)) {
		/* Fill input event structure according to detected event */
		switch (e.type) {
		case CACA_EVENT_KEY_PRESS:
		case CACA_EVENT_KEY_RELEASE:
			event.type = EVENT_KEYBOARD;
			event.keyboard.key = caca_get_event_key_ch(&e);
			state.active = (e.type == CACA_EVENT_KEY_PRESS);
			input_report(&event, &state);
			break;
		case CACA_EVENT_QUIT:
			event.type = EVENT_QUIT;
			input_report(&event, NULL);
			break;
		default:
			break;
		}
	}
}

void caca_deinit(struct input_frontend *UNUSED(fe))
{
}

INPUT_START(caca)
	.init = caca_init,
	.update = caca_update,
	.deinit = caca_deinit
INPUT_END

