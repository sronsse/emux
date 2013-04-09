#include <stdlib.h>
#include <list.h>

void list_insert(struct list_link **list, void *data)
{
	struct list_link *link;
	struct list_link *tail;

	/* Create new link */
	link = malloc(sizeof(struct list_link));
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

void *list_get_next(struct list_link **link)
{
	void *data = NULL;

	/* Grab data and move to next link */
	if (*link) {
		data = (*link)->data;
		*link = (*link)->next;
	}

	return data;
}

void list_remove(struct list_link **list, void *data)
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

void list_remove_all(struct list_link **list)
{
	struct list_link *link;
	while (*list) {
		link = *list;
		*list = (*list)->next;
		free(link);
	}
}

