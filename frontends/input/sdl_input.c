#include <stdio.h>
#include <stdbool.h>
#include <SDL.h>
#include <input.h>
#include <util.h>

static void sdl_update(struct input_frontend *fe);
static void key_event(SDL_Event *e);
static void mouse_event(SDL_Event *e);
static void quit_event();

void key_event(SDL_Event *e)
{
	struct input_event event;
	bool down;

	/* Fill event information (SDL codes match input system codes) */
	down = (e->key.state == SDL_PRESSED);
	event.device = DEVICE_KEYBOARD;
	event.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;
	event.code = e->key.keysym.sym;

	/* Report event */
	input_report(&event);
}

void mouse_event(SDL_Event *e)
{
	struct input_event event;
	bool down;

	/* Fill event information (SDL codes match input system codes) */
	down = (e->button.state == SDL_PRESSED);
	event.device = DEVICE_MOUSE;
	event.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;
	event.code = e->button.button;

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

void sdl_update(struct input_frontend *UNUSED(fe))
{
	SDL_Event e;

	/* Poll all events out of queue */
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			key_event(&e);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			mouse_event(&e);
			break;
		case SDL_QUIT:
			quit_event();
			break;
		default:
			break;
		}
	}
}

INPUT_START(sdl)
	.update = sdl_update
INPUT_END

