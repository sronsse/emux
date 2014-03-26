#ifndef _INPUT_H
#define _INPUT_H

#include <stdbool.h>
#include <list.h>
#include <video.h>

#define INPUT_START(_name) \
	static struct input_frontend _input_frontend = { \
		.name = #_name,
#define INPUT_END \
	}; \
	__attribute__((constructor)) static void _register() \
	{ \
		list_insert(&input_frontends, &_input_frontend); \
	} \
	__attribute__((destructor)) static void _unregister() \
	{ \
		list_remove(&input_frontends, &_input_frontend); \
	}

typedef void input_data_t;
typedef void input_priv_data_t;

struct input_state {
	bool active;
};

enum input_event_type {
	EVENT_KEYBOARD,
	EVENT_QUIT
};

struct input_event {
	enum input_event_type type;
	union {
		struct {
			int key;
		} keyboard;
	};
};

typedef void (*input_cb_t)(int id, struct input_state *state, input_data_t *d);

struct input_config {
	int num_events;
	struct input_event *events;
	input_cb_t callback;
	input_data_t *data;
};

struct input_frontend {
	char *name;
	input_priv_data_t *priv_data;
	bool (*init)(struct input_frontend *fe, window_t *window);
	void (*update)(struct input_frontend *fe);
	void (*deinit)(struct input_frontend *fe);
};

bool input_init(char *name, window_t *window);
bool input_load(char *name, struct input_event *events, int num_events);
void input_update();
void input_report(struct input_event *event, struct input_state *state);
void input_register(struct input_config *config);
void input_unregister(struct input_config *config);
void input_deinit();

extern struct list_link *input_frontends;

#endif

