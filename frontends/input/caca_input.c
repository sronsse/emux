#include <stdio.h>
#include <stdbool.h>
#include <caca.h>
#include <input.h>
#include <util.h>

static bool caca_init(video_window_t *window);
static void caca_deinit();

static caca_display_t *dp;

bool caca_init(video_window_t *window)
{
	dp = window;
	return true;
}

void caca_update()
{
	caca_event_t e;
	struct input_event event;

	/* Poll all events out of queue */
	while (caca_get_event(dp, CACA_EVENT_ANY, &e, 0)) {
		/* Fill input event structure according to detected event */
		switch (e.type) {
		case CACA_EVENT_QUIT:
			event.type = EVENT_QUIT;
			input_report(&event, NULL);
			break;
		default:
			break;
		}
	}
}

void caca_deinit()
{
}

INPUT_START(caca)
	.init = caca_init,
	.update = caca_update,
	.deinit = caca_deinit
INPUT_END

