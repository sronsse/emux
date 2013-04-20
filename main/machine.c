#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <clock.h>
#include <controller.h>
#include <cpu.h>
#include <input.h>
#include <machine.h>
#include <memory.h>
#include <util.h>

#define NS(s) (s * 1000000000)

static void machine_input_event(int id, struct input_state *state,
	input_data_t *data);

extern struct machine __machines_begin, __machines_end;
struct machine *machine;

bool machine_init(char *name)
{
	struct machine *m;
	for (m = &__machines_begin; m < &__machines_end; m++)
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

	if (machine->init)
		return machine->init();
	return true;
}

void machine_input_event(int UNUSED(id), struct input_state *UNUSED(state),
	input_data_t *UNUSED(data))
{
	machine->running = false;
}

void machine_run()
{
	uint64_t counter = 0;
	struct timespec start_time;
	struct timespec current_time;
	unsigned int mach_delay;
	unsigned int real_delay;
	struct input_config input_config;
	struct input_event quit_event;

	/* Return if no clock has been registered */
	if (machine->clock_rate == 0) {
		fprintf(stderr, "No clock registered for this machine!\n");
		return;
	}

	fprintf(stdout, "Machine clock rate: %luHz\n", machine->clock_rate);

	/* Compute machine delay between two ticks (in ns) */
	mach_delay = NS(1) / machine->clock_rate;

	/* Initialize start time */
	clock_gettime(CLOCK_MONOTONIC, &start_time);

	/* Set running flag and register for quit events */
	machine->running = true;
	quit_event.type = EVENT_QUIT;
	input_config.events = &quit_event;
	input_config.num_events = 1;
	input_config.callback = machine_input_event;
	input_register(&input_config);

	/* Run until user quits */
	while (machine->running) {
		/* Update input sub-system */
		input_update();

		/* Tick all registered clocks */
		clock_tick_all(counter++);

		/* Get actual delay (in ns) */
		clock_gettime(CLOCK_MONOTONIC, &current_time);
		real_delay = NS(current_time.tv_sec) + current_time.tv_nsec -
			NS(start_time.tv_sec) - start_time.tv_nsec;

		/* Sleep to match machine delay */
		if (counter * mach_delay > real_delay)
			usleep((counter * mach_delay - real_delay) / 1000);

		/* Reset counter and start time if needed */
		if (counter == machine->clock_rate) {
			clock_gettime(CLOCK_MONOTONIC, &start_time);
			counter = 0;
		}
	}

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

