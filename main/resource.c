#include <stdlib.h>
#include <resource.h>

struct resource *resource_get(char *name, enum resource_type type,
	struct resource *resources, int num_resources)
{
	int i;
	for (i = 0; i < num_resources; i++)
		if (!strcmp(name, resources[i].name) &&
			(type == resources[i].type))
			return &resources[i];
	return NULL;
}

