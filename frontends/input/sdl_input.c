#include <stdio.h>
#include <stdbool.h>
#include <SDL.h>
#include <input.h>
#include <log.h>
#include <util.h>

static bool sdl_init(struct input_frontend *fe, window_t *window);
static void sdl_update(struct input_frontend *fe);
static void sdl_deinit(struct input_frontend *fe);
static void key_event(SDL_KeyboardEvent *e);
static void mouse_event(SDL_MouseButtonEvent *e);
static void joy_button_event(SDL_JoyButtonEvent *e);
static void quit_event();

void key_event(SDL_KeyboardEvent *e)
{
	struct input_event event;
	bool down;

	/* Fill event information (SDL codes match input system codes) */
	down = (e->state == SDL_PRESSED);
	event.device = DEVICE_KEYBOARD;
	event.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;
	event.code = e->keysym.sym;

	/* Report event */
	input_report(&event);
}

void mouse_event(SDL_MouseButtonEvent *e)
{
	struct input_event event;
	bool down;

	/* Fill event information (SDL codes match input system codes) */
	down = (e->state == SDL_PRESSED);
	event.device = DEVICE_MOUSE;
	event.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;
	event.code = e->button;

	/* Report event */
	input_report(&event);
}

void joy_button_event(SDL_JoyButtonEvent *e)
{
	struct input_event event;
	bool down;

	/* Fill event information */
	down = (e->state == SDL_PRESSED);
	event.device = DEVICE_JOY_BUTTON;
	event.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;
	event.code = (e->which & JOY_BUTTON_DEV_MASK) << JOY_BUTTON_DEV_SHIFT;
	event.code |= (e->button & JOY_BUTTON_BTN_MASK) << JOY_BUTTON_BTN_SHIFT;

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

bool sdl_init(struct input_frontend *fe, window_t *UNUSED(window))
{
	struct list_link *joysticks = NULL;
	SDL_Joystick *joystick;
	int num;
	int i;

	/* Initialize SDL joystick subsystem */
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
		LOG_W("Could not initialize SDL joystick subsystem!\n");
		return true;
	}

	/* Enable joystick events */
	SDL_JoystickEventState(SDL_ENABLE);

	/* Open joystick handles */
	num = SDL_NumJoysticks();
	for (i = 0; i < num; i++) {
		joystick = SDL_JoystickOpen(i);
		list_insert(&joysticks, joystick);
		LOG_D("%s enabled.\n", SDL_JoystickName(i));
	}

	/* Save joysticks list */
	fe->priv_data = joysticks;
	return true;
}

void sdl_update(struct input_frontend *UNUSED(fe))
{
	SDL_Event e;

	/* Poll all events out of queue */
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			key_event(&e.key);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			mouse_event(&e.button);
			break;
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			joy_button_event(&e.jbutton);
			break;
		case SDL_QUIT:
			quit_event();
			break;
		default:
			break;
		}
	}
}

void sdl_deinit(struct input_frontend *fe)
{
	struct list_link *joysticks = fe->priv_data;
	struct list_link *link = joysticks;
	SDL_Joystick *joystick;

	/* Close all joystick handles */
	while ((joystick = list_get_next(&link)))
		SDL_JoystickClose(joystick);
	list_remove_all(&joysticks);
	fe->priv_data = NULL;

	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

INPUT_START(sdl)
	.init = sdl_init,
	.update = sdl_update,
	.deinit = sdl_deinit
INPUT_END

