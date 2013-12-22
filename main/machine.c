#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clock.h>
#include <cmdline.h>
#include <controller.h>
#include <cpu.h>
#include <input.h>
#include <log.h>
#include <machine.h>
#include <memory.h>
#include <util.h>

static void machine_input_event(int id,	struct input_state *state,
	input_data_t *data);

static char *machine_name;
PARAM(machine_name, string, "machine", NULL, "Selects machine to emulate");

struct list_link *machines;
static struct machine *machine;

void machine_input_event(int UNUSED(id), struct input_state *UNUSED(state),
	input_data_t *UNUSED(data))
{
	/* Request machine to stop running */
	machine->running = false;
}

bool machine_init()
{
	struct list_link *link = machines;
	struct machine *m;

	/* Validate machine option */
	if (!machine_name) {
		LOG_E("No machine selected!\n");
		return false;
	}

	while ((m = list_get_next(&link)))
		if (!strcmp(machine_name, m->name))
			machine = m;

	/* Exit if machine has not been found */
	if (!machine) {
		LOG_E("Machine \"%s\" not recognized!\n", machine_name);
		return false;
	}

	/* Display machine name and description */
	LOG_I("Machine: %s (%s)\n", machine->name, machine->description);

	if (machine->init && !machine->init(machine)) {
		/* Remove all components which may have been added */
		clock_remove_all();
		cpu_remove_all();
		controller_remove_all();
		memory_bus_remove_all();

		/* Print machine-specific options */
		cmdline_print_module_options(machine_name);
		return false;
	}

	return true;
}

void machine_reset()
{
	/* Call machine-specific reset if available */
	if (machine && machine->reset)
		machine->reset(machine);

	/* Reset CPUs, controllers, and clock system */
	cpu_reset_all();
	controller_reset_all();
	clock_reset();

	LOG_I("Machine reset.\n");
}

void machine_run()
{
	struct input_config input_config;
	struct input_event quit_event;

	/* Reset machine first */
	machine_reset();

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
	memory_bus_remove_all();
	if (machine && machine->deinit)
		machine->deinit(machine);
}

