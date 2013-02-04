#include <stdio.h>
#include <stdlib.h>
#include <machine.h>

extern struct machine __machines_begin, __machines_end;
struct machine *machine;

bool machine_init(char *name)
{
	struct machine *m;
	for (m = &__machines_begin; m < &__machines_end; m++)
		if (!strcmp(name, m->name))
			machine = m;

	if (!machine) {
		fprintf(stderr, "Machine \"%s\" not recognized!\n", name);
		return false;
	}

	return machine->init();
}

void machine_deinit()
{
	machine->deinit();
}

