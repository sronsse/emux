#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <clock.h>

#define NS(s) ((s) * 1000000000)

static uint64_t gcd(uint64_t a, uint64_t b);
static uint64_t lcm(uint64_t a, uint64_t b);
static uint64_t lcmm(int clock_index);

static struct clock **clocks;
static int num_clocks;
static uint64_t machine_clock_rate;
static uint64_t current_cycle;
static unsigned int mach_delay;
static struct clock *current_clock;

uint64_t gcd(uint64_t a, uint64_t b)
{
	uint64_t t;
	while (b) {
		t = b;
		b = a % b;
		a = t;
	}
	return a;
}

uint64_t lcm(uint64_t a, uint64_t b)
{
	return a * b / gcd(a, b);
}

uint64_t lcmm(int clock_index)
{
	struct clock *clock1 = clocks[clock_index];
	struct clock *clock2 = clocks[clock_index + 1];

	if (clock_index + 2 == num_clocks)
		return lcm(clock1->rate, clock2->rate);

	return lcm(clock1->rate, lcmm(clock_index + 1));
}

void clock_add(struct clock *clock)
{
	int i;

	/* Grow clocks array and insert clock */
	clocks = realloc(clocks, ++num_clocks * sizeof(struct clock *));
	clocks[num_clocks - 1] = clock;

	/* Update machine rate */
	machine_clock_rate = (num_clocks > 1) ? lcmm(0) : clock->rate;

	/* Update clock dividers */
	for (i = 0; i < num_clocks; i++)
		clocks[i]->div = machine_clock_rate / clocks[i]->rate;

	/* Set initial number of remaining clock cycles */
	clock->num_remaining_cycles = 0;

	/* Update machine delay between two ticks (in ns) */
	mach_delay = NS(1) / machine_clock_rate;
}

void clock_tick_all()
{
	int i;
	static struct timeval start_time;
	static struct timeval current_time;
	static unsigned int real_delay;

	/* Reset start time if needed */
	if (current_cycle++ == 0)
		gettimeofday(&start_time, NULL);

	for (i = 0; i < num_clocks; i++) {
		/* Set current clock */
		current_clock = clocks[i];

		/* Check if clock needs to be ticked */
		if (current_cycle % current_clock->div != 0)
			continue;

		/* Only execute clock action if there are no remaining cycles */
		if (current_clock->num_remaining_cycles == 0)
			current_clock->tick(current_clock->data);

		/* Decrement number of remaining number of cycles */
		current_clock->num_remaining_cycles--;

		/* Verify clock actually consumed cycles */
		if (current_clock->num_remaining_cycles < 0)
			fprintf(stderr,
				"Error: clock action should consume cycles!\n");
	}

	/* No clock is being ticked anymore */
	current_clock = NULL;

	/* Get actual delay (in ns) */
	gettimeofday(&current_time, NULL);
	real_delay = NS(current_time.tv_sec - start_time.tv_sec) +
		(current_time.tv_usec - start_time.tv_usec) * 1000;

	/* Sleep to match machine delay */
	if (current_cycle * mach_delay > real_delay)
		usleep((current_cycle * mach_delay - real_delay) / 1000);

	/* Reset current cycle if needed */
	if (current_cycle == machine_clock_rate)
		current_cycle = 0;
}

void clock_consume(int num_cycles)
{
	/* Verify we are not trying to consume cycles outside a tick function */
	if (!current_clock)
		fprintf(stderr, "Error: no clock is currently being ticked!\n");

	/* Increase number of remaining clock cycles by desired amount */
	current_clock->num_remaining_cycles += num_cycles;
}

void clock_remove_all()
{
	free(clocks);
}

