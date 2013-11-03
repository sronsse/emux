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
static struct timeval start_time;
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

void clock_prepare()
{
	/* Initialize start time */
	gettimeofday(&start_time, NULL);
}

void clock_tick_all()
{
	unsigned int real_delay;
	int num_remaining_cycles;
	struct timeval current_time;
	int i;

	/* Tick clocks */
	for (i = 0; i < num_clocks; i++) {
		/* Set current clock */
		current_clock = clocks[i];

		/* Only execute clock action if there are no remaining cycles */
		if (current_clock->num_remaining_cycles == 0)
			current_clock->tick(current_clock->data);
	}

	/* No clock is being ticked anymore */
	current_clock = NULL;

	/* Find minimum number of remaining cycles */
	num_remaining_cycles = clocks[0]->num_remaining_cycles;
	for (i = 1; i < num_clocks; i++)
		if (clocks[i]->num_remaining_cycles < num_remaining_cycles)
			num_remaining_cycles = clocks[i]->num_remaining_cycles;

	/* Sanity check (clocks should consume cycles at all times) */
	if (num_remaining_cycles == 0)
		fprintf(stderr, "Error: clock action should consume cycles!\n");

	/* Increment current cycle by min number of remaining cycles found */
	current_cycle += num_remaining_cycles;

	/* Decrement clocks remaining cycles */
	for (i = 0; i < num_clocks; i++)
		clocks[i]->num_remaining_cycles -= num_remaining_cycles;

	/* Get actual delay (in ns) */
	gettimeofday(&current_time, NULL);
	real_delay = NS(current_time.tv_sec - start_time.tv_sec) +
		(current_time.tv_usec - start_time.tv_usec) * 1000;

	/* Sleep to match machine delay */
	if (current_cycle * mach_delay > real_delay)
		usleep((current_cycle * mach_delay - real_delay) / 1000);

	/* Reset current cycle and start time if needed */
	if (current_cycle >= machine_clock_rate) {
		gettimeofday(&start_time, NULL);
		current_cycle -= machine_clock_rate;
	}
}

void clock_consume(int num_cycles)
{
	/* Increase number of remaining clock cycles by desired amount */
	current_clock->num_remaining_cycles += num_cycles * current_clock->div;
}

void clock_remove_all()
{
	free(clocks);
}

