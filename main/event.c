#include <stdlib.h>
#include <string.h>
#include <event.h>
#include <list.h>
#include <log.h>

struct listener {
	event_callback_t cb;
	event_data_t *data;
};

struct event {
	char *name;
	struct list_link *listeners;
};

static struct list_link *events;

void event_fire(char *name)
{
	struct list_link *link;
	struct event *event;
	struct listener *listener;

	/* Parse events and find match if any */
	link = events;
	while ((event = list_get_next(&link)))
		if (!strcmp(event->name, name))
			break;

	/* Call all listeners if event was found */
	if (event) {
		link = event->listeners;
		while ((listener = list_get_next(&link)))
			listener->cb(listener->data);
	}
}

void event_add(char *name, event_callback_t cb, event_data_t *data)
{
	struct list_link *link;
	struct event *event;
	struct listener *listener;

	/* Try finding matching event in event list */
	link = events;
	while ((event = list_get_next(&link)))
		if (!strcmp(event->name, name))
			break;

	/* Create new event if needed */
	if (!event) {
		event = malloc(sizeof(struct event));
		event->name = name;
		event->listeners = NULL;
		list_insert(&events, event);
	}

	/* Parse event listeners */
	link = event->listeners;
	while ((listener = list_get_next(&link))) {
		/* Skip listener in case it does not match */
		if (listener->cb != cb)
			continue;

		/* Warn and return if listener is already registered */
		LOG_W("Listener %p already registered to event \"%s\"!\n",
			cb,
			name);
		return;
	}

	/* Add listener to event */
	listener = malloc(sizeof(struct listener));
	listener->cb = cb;
	listener->data = data;
	list_insert(&event->listeners, listener);
	LOG_D("Registered %p listener to event \"%s\".\n", listener->cb, name);
}

void event_remove(char *name, event_callback_t cb)
{
	struct list_link *link;
	struct event *event;
	struct listener *listener;

	/* Try finding matching event in event list */
	link = events;
	while ((event = list_get_next(&link)))
		if (!strcmp(event->name, name))
			break;

	/* Warn and return if event was not found */
	if (!event) {
		LOG_W("Could not unregister %p from event \"%s\"!\n", cb, name);
		return;
	}

	/* Find listener */
	link = event->listeners;
	while ((listener = list_get_next(&link)))
		if (listener->cb == cb)
			break;

	/* Warn and return if listener was not found */
	if (!listener) {
		LOG_W("Could not unregister %p from event \"%s\"!\n", cb, name);
		return;
	}

	/* Free listener and remove it from list */
	free(listener);
	list_remove(&event->listeners, listener);
	LOG_D("Unregistered %p listener from event \"%s\".\n", cb, name);
}

void event_remove_all()
{
	struct list_link *event_link;
	struct list_link *listener_link;
	struct event *event;
	struct listener *listener;

	/* Parse event list */
	event_link = events;
	while ((event = list_get_next(&event_link))) {
		/* Free listeners */
		listener_link = event->listeners;
		while ((listener = list_get_next(&listener_link)))
			free(listener);

		/* Remove links from listener list */
		list_remove_all(&event->listeners);

		/* Free event */
		free(event);
	}

	/* Remove links from event list */
	list_remove_all(&events);
}

