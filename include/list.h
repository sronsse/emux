#ifndef _LIST_H
#define _LIST_H

#include <stdlib.h>

struct list_link {
	void *data;
	struct list_link *next;
};

static inline void list_insert(struct list_link **list, void *data)
{
	struct list_link *link;
	struct list_link *tail;

	/* Create new link */
	link = calloc(1, sizeof(struct list_link));
	link->data = data;
	link->next = NULL;

	/* Set head if needed */
	if (!*list) {
		*list = link;
		return;
	}

	/* Find tail and add link */
	tail = *list;
	while (tail->next)
		tail = tail->next;
	tail->next = link;
}

static inline void list_insert_before(struct list_link **list, void *data)
{
	struct list_link *link;

	/* Use regular method if list is empty */
	if (!*list) {
		list_insert(list, data);
		return;
	}

	/* Create new link duplicating head */
	link = calloc(1, sizeof(struct list_link));
	link->data = (*list)->data;
	link->next = (*list)->next;

	/* Change head data and insert link */
	(*list)->data = data;
	(*list)->next = link;
}

static inline void *list_get_next(struct list_link **link)
{
	void *data;

	/* Grab data and move to next link */
	if (*link) {
		data = (*link)->data;
		*link = (*link)->next;
		return data;
	}

	return NULL;
}

static inline void list_remove(struct list_link **list, void *data)
{
	struct list_link *link;
	struct list_link *next;

	/* Return if list is empty */
	if (!*list)
		return;

	/* Special case if data is in the first link */
	if ((*list)->data == data) {
		next = (*list)->next;
		free(*list);
		*list = next;
		return;
	}

	/* Find link to delete */
	link = *list;
	while (link->next) {
		if (link->next->data == data) {
			next = link->next->next;
			free(link->next);
			link->next = next;
			return;
		}
		link = link->next;
	}
}

static inline void list_remove_all(struct list_link **list)
{
	struct list_link *link;
	while (*list) {
		link = *list;
		*list = (*list)->next;
		free(link);
	}
}

#endif

