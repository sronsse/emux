#ifndef _INPUT_H
#define _INPUT_H

#include <stdbool.h>
#include <video.h>

#define INPUT_START(_name) \
	static struct input_frontend input_##_name \
		__attribute__(( \
			__used__, \
			__section__("input_frontends"), \
			__aligned__(__alignof__(struct input_frontend)))) = { \
		.name = #_name,
#define INPUT_END \
	};

typedef void input_data_t;

enum input_event_type {
	EVENT_KEYBOARD,
	EVENT_QUIT
};

struct input_state {
	bool active;
};

struct input_event {
	enum input_event_type type;
	union {
		struct {
			int key;
		} keyboard;
	};
};

struct input_config {
	int num_events;
	struct input_event *events;
	void (*callback)(int id, struct input_state *state, input_data_t *data);
	input_data_t *data;
};

struct input_frontend {
	char *name;
	bool (*init)(video_window_t *window);
	void (*update)();
	void (*deinit)();
};

bool input_init(char *name, video_window_t *window);
void input_update();
void input_report(struct input_event *event, struct input_state *state);
void input_register(struct input_config *config);
void input_unregister(struct input_config *config);
void input_deinit();

#endif

