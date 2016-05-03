#ifndef _EVENT_H
#define _EVENT_H

typedef void event_data_t;
typedef void (*event_callback_t) (event_data_t *data);

void event_fire(char *name);
void event_add(char *name, event_callback_t cb, event_data_t *data);
void event_remove(char *name, event_callback_t cb);
void event_remove_all();

#endif

