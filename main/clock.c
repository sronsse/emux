#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <clock.h>
#include <machine.h>

static uint64_t gcd(uint64_t a, uint64_t b);
static uint64_t lcm(uint64_t a, uint64_t b);
static uint64_t lcmm(struct clock_link *clocks);

extern struct machine *machine;

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

uint64_t lcmm(struct clock_link *clocks)
{
	if (!clocks->next->next)
		return lcm(clocks->clock->rate, clocks->next->clock->rate);
	return lcm(clocks->clock->rate, lcmm(clocks->next));
}

void clock_add(struct clock *clock)
{
	struct clock_link *link;
	struct clock_link *tail;

	/* Create new link */
	link = malloc(sizeof(struct clock_link));
	link->clock = clock;
	link->next = NULL;

	/* Insert link */
	if (!machine->clocks) {
		machine->clocks = link;
	} else {
		tail = machine->clocks;
		while (tail->next)
			tail = tail->next;
		tail->next = link;
	}

	/* Update machine rate */
	link = machine->clocks;
	machine->clock_rate = link->next ? lcmm(link) : clock->rate;

	/* Update clock dividers */
	while (link) {
		link->clock->div = machine->clock_rate / link->clock->rate;
		link = link->next;
	}
}

void clock_tick_all(uint64_t cycle)
{
	struct clock_link *link = machine->clocks;
	while (link) {
		if (cycle % link->clock->div == 0)
			link->clock->tick(link->clock->data);
		link = link->next;
	}
}

void clock_remove_all()
{
	struct clock_link *link;
	while (machine->clocks) {
		link = machine->clocks;
		machine->clocks = machine->clocks->next;
		free(link);
	}
}

