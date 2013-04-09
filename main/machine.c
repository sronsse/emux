#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <clock.h>
#include <controller.h>
#include <cpu.h>
#include <machine.h>
#include <memory.h>

#define NS(s) (s * 1000000000)

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

void machine_run()
{
	uint64_t counter = 0;
	struct timespec start_time;
	struct timespec current_time;
	unsigned int mach_delay;
	unsigned int real_delay;

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

	/* Run forever */
	for (;;) {
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

