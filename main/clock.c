#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <clock.h>
#include <log.h>

#define NS(s) ((s) * 1000000000)

static struct clock **clocks;
static int num_clocks;
static float machine_clock_rate;
static float mach_delay;
static float current_cycle;
static float num_remaining_cycles;
static struct timeval start_time;
struct clock *current_clock;

void clock_add(struct clock *clock)
{
	int i;

	/* Grow clocks array and insert clock */
	clocks = realloc(clocks, ++num_clocks * sizeof(struct clock *));
	clocks[num_clocks - 1] = clock;

	/* Update machine rate/delay if needed */
	if (clock->rate > machine_clock_rate) {
		machine_clock_rate = clock->rate;
		mach_delay = NS(1) / machine_clock_rate;
	}

	/* Set clock dividers */
	for (i = 0; i < num_clocks; i++)
		clocks[i]->div = machine_clock_rate / clocks[i]->rate;
}

void clock_reset()
{
	int i;

	/* Initialize current cycle and start time */
	current_cycle = 0.0f;
	gettimeofday(&start_time, NULL);

	/* Reset all clock remaining cycles */
	for (i = 0; i < num_clocks; i++)
		clocks[i]->num_remaining_cycles = 0.0f;
}

void clock_tick_all(bool handle_delay)
{
	float num_cycles;
	float real_delay;
	float d;
	struct timeval current_time;
	int i;

	/* Initialize number of cycles to skip */
	num_cycles = machine_clock_rate;

	/* Tick clocks */
	for (i = 0; i < num_clocks; i++) {
		/* Set current clock */
		current_clock = clocks[i];

		/* Skip clock if disabled */
		if (!current_clock->enabled)
			continue;

		/* Decrease clock cycles */
		current_clock->num_remaining_cycles -= num_remaining_cycles;

		/* Tick clock if necessary */
		if (current_clock->num_remaining_cycles <= 0.0f)
			current_clock->tick(current_clock->data);

		/* Save next number of remaining cycles if needed */
		if ((current_clock->num_remaining_cycles < num_cycles) &&
			current_clock->enabled)
			num_cycles = current_clock->num_remaining_cycles;
	}

	/* Update current cycle and number of remaining cycles */
	current_cycle += num_cycles;
	num_remaining_cycles = num_cycles;

	/* Only sleep if delay handling is needed */
	if (handle_delay) {
		/* Get actual delay (in ns) */
		gettimeofday(&current_time, NULL);
		real_delay = NS(current_time.tv_sec - start_time.tv_sec) +
			(current_time.tv_usec - start_time.tv_usec) * 1000;

		/* Sleep to match machine delay if needed */
		if (current_cycle * mach_delay > real_delay) {
			d = (current_cycle * mach_delay - real_delay) / 1000;
			usleep(d);
		}
	}

	/* Reset current cycle and start time if needed */
	if (current_cycle >= machine_clock_rate) {
		if (handle_delay)
			gettimeofday(&start_time, NULL);
		current_cycle -= machine_clock_rate;
	}
}

void clock_remove_all()
{
	free(clocks);
	clocks = NULL;
	num_clocks = 0;
}

