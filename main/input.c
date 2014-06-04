#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <config.h>
#include <cmdline.h>
#include <env.h>
#include <input.h>
#include <list.h>
#include <log.h>
#ifdef LIBRETRO
#undef CONFIG_INPUT_XML
#endif
#ifdef CONFIG_INPUT_XML
#include <roxml.h>
#endif

#define MAX_CODE_NAME_LENGTH 32

#ifdef CONFIG_INPUT_XML
/* Configuration file and node definitions */
#define MAX_DOC_PATH_LENGTH	1024
#define DOC_FILENAME		"config.xml"
#define DOC_CONFIG_NODE_NAME	"config"
#define DOC_KEY_NODE_NAME	"key"
#endif

static void get_key_code_name(int code, char *output);
static void get_mouse_code_name(int code, char *output);
static void get_joy_button_code_name(int code, char *output);
static void get_joy_hat_code_name(int code, char *output);
static void print_desc(struct input_desc *desc);

#ifdef CONFIG_INPUT_XML
static bool input_load(struct input_config *config);
#endif

struct list_link *input_frontends;
static struct input_frontend *frontend;
static struct list_link *configs;

void get_key_code_name(int code, char *output)
{
	char *name;

	switch (code) {
	case KEY_BACKSPACE:
		name = "backspace";
		break;
	case KEY_TAB:
		name = "tab";
		break;
	case KEY_CLEAR:
		name = "clear";
		break;
	case KEY_RETURN:
		name = "return";
		break;
	case KEY_PAUSE:
		name = "pause";
		break;
	case KEY_ESCAPE:
		name = "esc";
		break;
	case KEY_SPACE:
		name = "space";
		break;
	case KEY_EXCLAIM:
	case KEY_QUOTEDBL:
	case KEY_HASH:
	case KEY_DOLLAR:
	case KEY_AMPERSAND:
	case KEY_QUOTE:
	case KEY_LEFTPAREN:
	case KEY_RIGHTPAREN:
	case KEY_ASTERISK:
	case KEY_PLUS:
	case KEY_COMMA:
	case KEY_MINUS:
	case KEY_PERIOD:
	case KEY_SLASH:
	case KEY_0:
	case KEY_1:
	case KEY_2:
	case KEY_3:
	case KEY_4:
	case KEY_5:
	case KEY_6:
	case KEY_7:
	case KEY_8:
	case KEY_9:
	case KEY_COLON:
	case KEY_SEMICOLON:
	case KEY_LESS:
	case KEY_EQUALS:
	case KEY_GREATER:
	case KEY_QUESTION:
	case KEY_AT:
	case KEY_BACKSLASH:
	case KEY_RIGHTBRACKET:
	case KEY_CARET:
	case KEY_UNDERSCORE:
	case KEY_BACKQUOTE:
	case KEY_a:
	case KEY_b:
	case KEY_c:
	case KEY_d:
	case KEY_e:
	case KEY_f:
	case KEY_g:
	case KEY_h:
	case KEY_i:
	case KEY_j:
	case KEY_k:
	case KEY_l:
	case KEY_m:
	case KEY_n:
	case KEY_o:
	case KEY_p:
	case KEY_q:
	case KEY_r:
	case KEY_s:
	case KEY_t:
	case KEY_u:
	case KEY_v:
	case KEY_w:
	case KEY_x:
	case KEY_y:
	case KEY_z:
		sprintf(output, "%c", '!' + code - KEY_EXCLAIM);
		return;
	case KEY_DELETE:
		name = "delete";
		break;
	case KEY_WORLD_0:
	case KEY_WORLD_1:
	case KEY_WORLD_2:
	case KEY_WORLD_3:
	case KEY_WORLD_4:
	case KEY_WORLD_5:
	case KEY_WORLD_6:
	case KEY_WORLD_7:
	case KEY_WORLD_8:
	case KEY_WORLD_9:
	case KEY_WORLD_10:
	case KEY_WORLD_11:
	case KEY_WORLD_12:
	case KEY_WORLD_13:
	case KEY_WORLD_14:
	case KEY_WORLD_15:
	case KEY_WORLD_16:
	case KEY_WORLD_17:
	case KEY_WORLD_18:
	case KEY_WORLD_19:
	case KEY_WORLD_20:
	case KEY_WORLD_21:
	case KEY_WORLD_22:
	case KEY_WORLD_23:
	case KEY_WORLD_24:
	case KEY_WORLD_25:
	case KEY_WORLD_26:
	case KEY_WORLD_27:
	case KEY_WORLD_28:
	case KEY_WORLD_29:
	case KEY_WORLD_30:
	case KEY_WORLD_31:
	case KEY_WORLD_32:
	case KEY_WORLD_33:
	case KEY_WORLD_34:
	case KEY_WORLD_35:
	case KEY_WORLD_36:
	case KEY_WORLD_37:
	case KEY_WORLD_38:
	case KEY_WORLD_39:
	case KEY_WORLD_40:
	case KEY_WORLD_41:
	case KEY_WORLD_42:
	case KEY_WORLD_43:
	case KEY_WORLD_44:
	case KEY_WORLD_45:
	case KEY_WORLD_46:
	case KEY_WORLD_47:
	case KEY_WORLD_48:
	case KEY_WORLD_49:
	case KEY_WORLD_50:
	case KEY_WORLD_51:
	case KEY_WORLD_52:
	case KEY_WORLD_53:
	case KEY_WORLD_54:
	case KEY_WORLD_55:
	case KEY_WORLD_56:
	case KEY_WORLD_57:
	case KEY_WORLD_58:
	case KEY_WORLD_59:
	case KEY_WORLD_60:
	case KEY_WORLD_61:
	case KEY_WORLD_62:
	case KEY_WORLD_63:
	case KEY_WORLD_64:
	case KEY_WORLD_65:
	case KEY_WORLD_66:
	case KEY_WORLD_67:
	case KEY_WORLD_68:
	case KEY_WORLD_69:
	case KEY_WORLD_70:
	case KEY_WORLD_71:
	case KEY_WORLD_72:
	case KEY_WORLD_73:
	case KEY_WORLD_74:
	case KEY_WORLD_75:
	case KEY_WORLD_76:
	case KEY_WORLD_77:
	case KEY_WORLD_78:
	case KEY_WORLD_79:
	case KEY_WORLD_80:
	case KEY_WORLD_81:
	case KEY_WORLD_82:
	case KEY_WORLD_83:
	case KEY_WORLD_84:
	case KEY_WORLD_85:
	case KEY_WORLD_86:
	case KEY_WORLD_87:
	case KEY_WORLD_88:
	case KEY_WORLD_89:
	case KEY_WORLD_90:
	case KEY_WORLD_91:
	case KEY_WORLD_92:
	case KEY_WORLD_93:
	case KEY_WORLD_94:
	case KEY_WORLD_95:
		sprintf(output, "world %u", code - KEY_WORLD_0);
		return;
	case KEY_KP0:
	case KEY_KP1:
	case KEY_KP2:
	case KEY_KP3:
	case KEY_KP4:
	case KEY_KP5:
	case KEY_KP6:
	case KEY_KP7:
	case KEY_KP8:
	case KEY_KP9:
		sprintf(output, "%u (numpad)", code - KEY_KP0);
		return;
	case KEY_KP_PERIOD:
		name = ". (num)";
		break;
	case KEY_KP_DIVIDE:
		name = "/ (num)";
		break;
	case KEY_KP_MULTIPLY:
		name = "* (num)";
		break;
	case KEY_KP_MINUS:
		name = "- (num)";
		break;
	case KEY_KP_PLUS:
		name = "+ (num)";
		break;
	case KEY_KP_ENTER:
		name = "enter (num)";
		break;
	case KEY_KP_EQUALS:
		name = "= (num)";
		break;
	case KEY_UP:
		name = "up";
		break;
	case KEY_DOWN:
		name = "down";
		break;
	case KEY_RIGHT:
		name = "right";
		break;
	case KEY_LEFT:
		name = "left";
		break;
	case KEY_INSERT:
		name = "insert";
		break;
	case KEY_HOME:
		name = "home";
		break;
	case KEY_END:
		name = "end";
		break;
	case KEY_PAGEUP:
		name = "page up";
		break;
	case KEY_PAGEDOWN:
		name = "page down";
		break;
	case KEY_F1:
	case KEY_F2:
	case KEY_F3:
	case KEY_F4:
	case KEY_F5:
	case KEY_F6:
	case KEY_F7:
	case KEY_F8:
	case KEY_F9:
	case KEY_F10:
	case KEY_F11:
	case KEY_F12:
	case KEY_F13:
	case KEY_F14:
	case KEY_F15:
		sprintf(output, "F%u", code + 1 - KEY_F1);
		return;
	case KEY_NUMLOCK:
		name = "num lock";
		break;
	case KEY_CAPSLOCK:
		name = "caps lock";
		break;
	case KEY_SCROLLOCK:
		name = "scroll lock";
		break;
	case KEY_RSHIFT:
		name = "right shift";
		break;
	case KEY_LSHIFT:
		name = "left shift";
		break;
	case KEY_RCTRL:
		name = "right ctrl";
		break;
	case KEY_LCTRL:
		name = "left ctrl";
		break;
	case KEY_RALT:
		name = "right alt";
		break;
	case KEY_LALT:
		name = "left alt";
		break;
	case KEY_RMETA:
		name = "right meta";
		break;
	case KEY_LMETA:
		name = "left meta";
		break;
	case KEY_LSUPER:
		name = "left super";
		break;
	case KEY_RSUPER:
		name = "right super";
		break;
	case KEY_MODE:
		name = "mode";
		break;
	case KEY_COMPOSE:
		name = "compose";
		break;
	case KEY_HELP:
		name = "help";
		break;
	case KEY_PRINT:
		name = "print";
		break;
	case KEY_SYSREQ:
		name = "sysreq";
		break;
	case KEY_BREAK:
		name = "break";
		break;
	case KEY_MENU:
		name = "menu";
		break;
	case KEY_POWER:
		name = "power";
		break;
	case KEY_EURO:
		name = "euro";
		break;
	case KEY_UNDO:
		name = "undo";
		break;
	default:
		name = "unknown";
		break;
	}

	strcpy(output, name);
}

void get_mouse_code_name(int code, char *output)
{
	char *name;

	switch (code) {
	case MOUSE_BUTTON_LEFT:
		name = "left button";
		break;
	case MOUSE_BUTTON_MIDDLE:
		name = "middle button";
		break;
	case MOUSE_BUTTON_RIGHT:
		name = "right button";
		break;
	case MOUSE_BUTTON_WHEELUP:
		name = "wheel up";
		break;
	case MOUSE_BUTTON_WHEELDOWN:
		name = "wheel up";
		break;
	default:
		name = "unknown";
		break;
	}

	strcpy(output, name);
}

void get_joy_button_code_name(int code, char *output)
{
	int device;
	int button;

	/* Get device and button from code */
	device = (code >> JOY_BUTTON_DEV_SHIFT) & JOY_BUTTON_DEV_MASK;
	button = (code >> JOY_BUTTON_BTN_SHIFT) & JOY_BUTTON_BTN_MASK;

	sprintf(output, "%u - %u", device, button);
}

void get_joy_hat_code_name(int code, char *output)
{
	int device;
	int dir;

	/* Get device and hat direction from code */
	device = (code >> JOY_BUTTON_DEV_SHIFT) & JOY_BUTTON_DEV_MASK;
	dir = (code >> JOY_BUTTON_BTN_SHIFT) & JOY_BUTTON_BTN_MASK;

	/* Fill output based on hat direction */
	switch (dir) {
	case JOY_HAT_UP:
		sprintf(output, "%u - up", device);
		break;
	case JOY_HAT_RIGHT:
		sprintf(output, "%u - right", device);
		break;
	case JOY_HAT_DOWN:
		sprintf(output, "%u - down", device);
		break;
	case JOY_HAT_LEFT:
		sprintf(output, "%u - left", device);
		break;
	default:
		sprintf(output, "%u - unknown", device);
		break;
	}
}

void print_desc(struct input_desc *desc)
{
	char code_name[MAX_CODE_NAME_LENGTH];

	switch (desc->device) {
	case DEVICE_KEYBOARD:
		get_key_code_name(desc->code, code_name);
		LOG_I("%s: key %s\n", desc->name, code_name);
		break;
	case DEVICE_MOUSE:
		get_mouse_code_name(desc->code, code_name);
		LOG_I("%s: mouse %s\n", desc->name, code_name);
		break;
	case DEVICE_JOY_BUTTON:
		get_joy_button_code_name(desc->code, code_name);
		LOG_I("%s: joy button %s\n", desc->name, code_name);
		break;
	case DEVICE_JOY_HAT:
		get_joy_hat_code_name(desc->code, code_name);
		LOG_I("%s: joy hat %s\n", desc->name, code_name);
		break;
	case DEVICE_NONE:
	default:
		break;
	}
}

bool input_init(char *name, window_t *window)
{
	struct list_link *link = input_frontends;
	struct input_frontend *fe;

	if (frontend) {
		LOG_E("Input frontend already initialized!\n");
		return false;
	}

	/* Find input frontend */
	while ((fe = list_get_next(&link))) {
		/* Skip if name does not match */
		if (strcmp(name, fe->name))
			continue;

		/* Initialize frontend */
		if (fe->init && !fe->init(fe, window))
			return false;

		/* Save frontend and return success */
		frontend = fe;
		return true;
	}

	/* Warn as input frontend was not found */
	LOG_E("Input frontend \"%s\" not recognized!\n", name);
	return false;
}

#ifdef CONFIG_INPUT_XML
bool input_load(struct input_config *config)
{
	char doc_path[MAX_DOC_PATH_LENGTH + 1];
	node_t *config_doc;
	node_t *node;
	node_t *child;
	int i;
	char *str;
	char *end;
	int key;
	int size;
	bool rc = false;

	LOG_D("Opening input configuration file.\n");

	/* Set config doc path */
	snprintf(doc_path,
		MAX_DOC_PATH_LENGTH,
		"%s/%s",
		env_get_config_path(),
		DOC_FILENAME);

	/* Load input config file and fall back to original path if needed */
	config_doc = roxml_load_doc(doc_path);
	if (!config_doc)
		config_doc = roxml_load_doc(DOC_FILENAME);

	/* Return if file could not be opened */
	if (!config_doc) {
		LOG_W("Could not open input configuration file!\n");
		return false;
	}

	/* Find document initial node */
	node = roxml_get_chld(config_doc, DOC_CONFIG_NODE_NAME, 0);
	if (!node)
		goto err;

	/* Find appropriate section */
	node = roxml_get_chld(node, name, 0);
	if (!node)
		goto err;

	/* Get number of entries and check for validity */
	if (roxml_get_chld_nb(node) != config->num_descs)
		goto err;

	/* Parse children and create matching descriptions */
	for (i = 0; i < config->num_descs; i++) {
		child = roxml_get_chld(node, NULL, i);
		if (!child)
			goto err;

		/* Check for event type */
		str = roxml_get_name(child, NULL, 0);
		if (!strcmp(str, DOC_KEY_NODE_NAME)) {
			str = roxml_get_content(child, NULL, 0, &size);

			/* Get code value */
			key = strtol(str, &end, 10);
			if (*end)
				goto err;

			/* Set event */
			events[i].type = EVENT_KEYBOARD;
			events[i].keyboard.key = key;
		} else {
			/* We should never reach here */
			goto err;
		}
	}

	/* Configuration was loaded properly */
	rc = true;
err:
	roxml_close(config_doc);
	roxml_release(RELEASE_ALL);
	if (!rc)
		LOG_W("Error parsing input configuration file!\n");
	return rc;
}
#endif

void input_update()
{
	if (frontend && frontend->update)
		frontend->update(frontend);
}

void input_report(struct input_event *event)
{
	struct list_link *link = configs;
	struct input_config *config;
	struct input_desc *desc;
	int i;

	/* Cycle through all registered input configurations */
	while ((config = list_get_next(&link)))
		for (i = 0; i < config->num_descs; i++) {
			/* Get config input description */
			desc = &config->descs[i];

			/* Skip if device is not matched */
			if (event->device != desc->device)
				continue;

			/* Skip if code is not matched */
			if (event->code != desc->code)
				continue;

			/* Call listener and switch to next config */
			config->callback(i, event->type, config->data);
			break;
		}
}

void input_register(struct input_config *config, bool restore)
{
	int i;

	/* Leave already if frontend is not initialized */
	if (!frontend)
		return;

#ifdef CONFIG_INPUT_XML
	/* Try restoring configuration if requested */
	if (restore)
		input_load(config);
#else
	(void)restore;
#endif

	/* Print configuration */
	for (i = 0; i < config->num_descs; i++)
		print_desc(&config->descs[i]);

	/* Notify frontend of new configuration */
	if (frontend->load)
		frontend->load(frontend, config);

	/* Append configuration */
	list_insert(&configs, config);
}

void input_unregister(struct input_config *config)
{
	if (frontend) {
		/* Unregister config from frontend */
		if (frontend->unload)
			frontend->unload(frontend, config);

		/* Remove config */
		list_remove(&configs, config);
	}
}

void input_deinit()
{
	if (!frontend)
		return;

	if (frontend->deinit)
		frontend->deinit(frontend);
	frontend = NULL;
}

