#ifndef _CLOCK_H
#define _CLOCK_H

#include <stdbool.h>
#include <stdint.h>

typedef void clock_data_t;
typedef void (*clock_tick_t)(clock_data_t *data);

struct clock {
	uint64_t rate;
	clock_data_t *data;
	uint64_t div;
	int num_remaining_cycles;
	clock_tick_t tick;
};

void clock_add(struct clock *clock);
void clock_reset();
void clock_tick_all(bool handle_delay);
void clock_consume(int num_cycles);
void clock_remove_all();

#endif

