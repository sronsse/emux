#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <clock.h>
#include <list.h>

#define NS(s) ((s) * 1000000000)

static uint64_t gcd(uint64_t a, uint64_t b);
static uint64_t lcm(uint64_t a, uint64_t b);
static uint64_t lcmm(struct list_link *clocks);

static struct list_link *clocks;
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

uint64_t lcmm(struct list_link *clocks)
{
	struct clock *clock1, *clock2;

	if (!clocks->next->next){
		clock1 = clocks->data;
		clock2 = clocks->next->data;
		return lcm(clock1->rate, clock2->rate);
	}

	clock1 = clocks->data;
	return lcm(clock1->rate, lcmm(clocks->next));
}

void clock_add(struct clock *clock)
{
	struct list_link *link;
	struct clock *c;

	list_insert(&clocks, clock);

	/* Update machine rate */
	machine_clock_rate = clocks->next ? lcmm(clocks) :
		clock->rate;

	/* Update clock dividers */
	link = clocks;
	while ((c = list_get_next(&link)))
		c->div = machine_clock_rate / c->rate;

	/* Set initial number of remaining clock cycles */
	clock->num_remaining_cycles = 0;

	/* Update machine delay between two ticks (in ns) */
	mach_delay = NS(1) / machine_clock_rate;
}

void clock_tick_all()
{
	struct list_link *link = clocks;
	struct clock *clock;
	static struct timeval start_time;
	static struct timeval current_time;
	static unsigned int real_delay;

	/* Reset start time if needed */
	if (current_cycle++ == 0)
		gettimeofday(&start_time, NULL);

	while ((clock = list_get_next(&link))) {
		/* Set current clock */
		current_clock = clock;

		/* Check if clock needs to be ticked */
		if (current_cycle % clock->div != 0)
			continue;

		/* Only execute clock action if there are no remaining cycles */
		if (clock->num_remaining_cycles == 0)
			clock->tick(clock->data);

		/* Decrement number of remaining number of cycles */
		clock->num_remaining_cycles--;

		/* Verify clock actually consumed cycles */
		if (clock->num_remaining_cycles < 0)
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
	list_remove_all(&clocks);
}

