#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <clock.h>
#include <machine.h>

static uint64_t gcd(uint64_t a, uint64_t b);
static uint64_t lcm(uint64_t a, uint64_t b);
static uint64_t lcmm(struct list_link *clocks);

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

	list_insert(&machine->clocks, clock);

	/* Update machine rate */
	machine->clock_rate = machine->clocks->next ? lcmm(machine->clocks) :
		clock->rate;

	/* Update clock dividers */
	link = machine->clocks;
	while ((c = list_get_next(&link)))
		c->div = machine->clock_rate / c->rate;
}

void clock_tick_all(uint64_t cycle)
{
	struct list_link *link = machine->clocks;
	struct clock *clock;

	while ((clock = list_get_next(&link)))
		if (cycle % clock->div == 0)
			clock->tick(clock->data);
}

void clock_remove_all()
{
	list_remove_all(&machine->clocks);
}

