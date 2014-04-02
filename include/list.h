#ifndef _LIST_H
#define _LIST_H

struct list_link {
	void *data;
	struct list_link *next;
};

void list_insert(struct list_link **list, void *data);
void list_insert_before(struct list_link **list, void *data);
void *list_get_next(struct list_link **link);
void list_remove(struct list_link **list, void *data);
void list_remove_all(struct list_link **list);

#endif

