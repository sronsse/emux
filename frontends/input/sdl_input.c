#include <stdbool.h>
#include <stdlib.h>
#include <SDL.h>
#include <input.h>
#include <log.h>
#include <util.h>

struct joy_data {
	SDL_Joystick *joystick;
	SDL_JoystickID id;
	Uint8 hat;
};

static bool sdl_init(struct input_frontend *fe, window_t *window);
static void sdl_update(struct input_frontend *fe);
static void sdl_deinit(struct input_frontend *fe);
static void key_event(SDL_KeyboardEvent *e);
static void mouse_event(SDL_MouseButtonEvent *e);
static void joy_button_event(SDL_JoyButtonEvent *e);
static void joy_hat_event(struct list_link *joysticks, SDL_JoyHatEvent *e);
static void quit_event();

void key_event(SDL_KeyboardEvent *e)
{
	struct input_event event;
	bool down;

	/* Fill event information (SDL codes match input system codes) */
	down = (e->state == SDL_PRESSED);
	event.device = DEVICE_KEYBOARD;
	event.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;

	/* Fill event code (SDL codes match apart from arrows) */
	switch (e->keysym.sym) {
	case SDLK_UP:
		event.code = KEY_UP;
		break;
	case SDLK_DOWN:
		event.code = KEY_DOWN;
		break;
	case SDLK_RIGHT:
		event.code = KEY_RIGHT;
		break;
	case SDLK_LEFT:
		event.code = KEY_LEFT;
		break;
	default:
		event.code = e->keysym.sym;
		break;
	}

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

void joy_hat_event(struct list_link *joysticks, SDL_JoyHatEvent *e)
{
	struct input_event event;
	struct joy_data *joy_data = NULL;
	bool down;
	int i;

	/* Fill event information */
	event.device = DEVICE_JOY_HAT;

	/* Skip to detected joystick */
	for (i = 0; i <= e->which; i++)
		joy_data = list_get_next(&joysticks);

	/* Set device */
	event.code = (e->which & JOY_HAT_DEV_MASK) << JOY_HAT_DEV_SHIFT;

	/* Report hat up if needed */
	if ((joy_data->hat & SDL_HAT_UP) != (e->value & SDL_HAT_UP)) {
		down = (e->value & SDL_HAT_UP);
		event.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;
		event.code |= JOY_HAT_UP;
		input_report(&event);
	}

	/* Report hat right if needed */
	if ((joy_data->hat & SDL_HAT_RIGHT) != (e->value & SDL_HAT_RIGHT)) {
		down = (e->value & SDL_HAT_RIGHT);
		event.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;
		event.code |= JOY_HAT_RIGHT;
		input_report(&event);
	}

	/* Report hat down if needed */
	if ((joy_data->hat & SDL_HAT_DOWN) != (e->value & SDL_HAT_DOWN)) {
		down = (e->value & SDL_HAT_DOWN);
		event.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;
		event.code |= JOY_HAT_DOWN;
		input_report(&event);
	}

	/* Report hat left if needed */
	if ((joy_data->hat & SDL_HAT_LEFT) != (e->value & SDL_HAT_LEFT)) {
		down = (e->value & SDL_HAT_LEFT);
		event.type = down ? EVENT_BUTTON_DOWN : EVENT_BUTTON_UP;
		event.code |= JOY_HAT_LEFT;
		input_report(&event);
	}

	/* Save hat value */
	joy_data->hat = e->value;
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
	struct joy_data *joy_data;
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
		joy_data = malloc(sizeof(struct joy_data));
		joy_data->joystick = SDL_JoystickOpen(i);
		joy_data->id = SDL_JoystickInstanceID(joy_data->joystick);
		joy_data->hat = SDL_HAT_CENTERED;
		list_insert(&joysticks, joy_data);
		LOG_D("%s enabled.\n", SDL_JoystickName(joy_data->joystick));
	}

	/* Save joysticks list */
	fe->priv_data = joysticks;
	return true;
}

void sdl_update(struct input_frontend *fe)
{
	struct list_link *joysticks = fe->priv_data;
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
		case SDL_JOYHATMOTION:
			joy_hat_event(joysticks, &e.jhat);
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
	struct joy_data *joy_data;

	/* Close all joystick handles */
	while ((joy_data = list_get_next(&link))) {
		SDL_JoystickClose(joy_data->joystick);
		free(joy_data);
	}
	list_remove_all(&joysticks);
	fe->priv_data = NULL;

	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

INPUT_START(sdl)
	.init = sdl_init,
	.update = sdl_update,
	.deinit = sdl_deinit
INPUT_END

