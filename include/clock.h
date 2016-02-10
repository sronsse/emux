#ifndef _CLOCK_H
#define _CLOCK_H

#include <stdbool.h>
#include <stdint.h>

typedef void clock_data_t;
typedef void (*clock_tick_t)(clock_data_t *data);

struct clock {
	float rate;
	float div;
	float num_remaining_cycles;
	bool enabled;
	clock_data_t *data;
	clock_tick_t tick;
};

void clock_add(struct clock *clock);
void clock_reset();
void clock_tick_all(bool handle_delay);
void clock_remove_all();

extern struct clock *current_clock;

static inline void clock_consume(int num_cycles)
{
	/* Increase number of remaining clock cycles by desired amount */
	current_clock->num_remaining_cycles += num_cycles * current_clock->div;
}

#endif

