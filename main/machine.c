#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clock.h>
#include <cmdline.h>
#include <controller.h>
#include <cpu.h>
#include <input.h>
#include <machine.h>
#include <memory.h>
#include <util.h>
#ifdef __APPLE__
#include <mach-o/getsect.h>
#endif

#ifdef __APPLE__
void cx_machines() __attribute__((__constructor__));
#endif

static void machine_input_event(int id, struct input_state *state,
	input_data_t *data);

#if defined(_WIN32)
extern struct machine _machines_begin, _machines_end;
static struct machine *machines_begin = &_machines_begin;
static struct machine *machines_end = &_machines_end;
#elif defined(__APPLE__)
struct machine *machines_begin;
struct machine *machines_end;
#else
extern struct machine __machines_begin, __machines_end;
static struct machine *machines_begin = &__machines_begin;
static struct machine *machines_end = &__machines_end;
#endif

struct machine *machine;

#ifdef __APPLE__
void cx_machines()
{
#ifdef __LP64__
	const struct section_64 *sect;
#else
	const struct section *sect;
#endif
	sect = getsectbyname(MACHINE_SEGMENT_NAME, MACHINE_SECTION_NAME);
	machines_begin = (struct machine *)(sect->addr);
	machines_end = (struct machine *)(sect->addr + sect->size);
}
#endif

bool machine_init()
{
	char *name;
	struct machine *m;

	/* Parse machine from command line */
	if (!cmdline_parse_string("machine", &name))
		return false;

	for (m = machines_begin; m < machines_end; m++)
		if (!strcmp(name, m->name))
			machine = m;

	/* Exit if machine has not been found */
	if (!machine) {
		fprintf(stderr, "Machine \"%s\" not recognized!\n", name);
		return false;
	}

	/* Display machine name and description */
	fprintf(stdout, "Machine: %s (%s)\n", machine->name,
		machine->description);

	if (machine->init && !machine->init())
		return false;

	return true;
}

void machine_input_event(int UNUSED(id), struct input_state *UNUSED(state),
	input_data_t *UNUSED(data))
{
	machine->running = false;
}

void machine_run()
{
	struct input_config input_config;
	struct input_event quit_event;

	/* Set running flag and register for quit events */
	machine->running = true;
	quit_event.type = EVENT_QUIT;
	input_config.events = &quit_event;
	input_config.num_events = 1;
	input_config.callback = machine_input_event;
	input_register(&input_config);

	/* Run until user quits */
	while (machine->running)
		clock_tick_all();

	/* Unregister quit events */
	input_unregister(&input_config);
}

void machine_deinit()
{
	clock_remove_all();
	cpu_remove_all();
	controller_remove_all();
	memory_region_remove_all();
	if (machine->deinit)
		machine->deinit();
}

