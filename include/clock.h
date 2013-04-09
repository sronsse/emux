#ifndef _CLOCK_H
#define _CLOCK_H

#include <stdint.h>

typedef void clock_data_t;

struct clock {
	uint64_t rate;
	clock_data_t *data;
	uint64_t div;
	void (*tick)(clock_data_t *clock_data);
};

void clock_add(struct clock *clock);
void clock_tick_all(uint64_t cycle);
void clock_remove_all();

#endif

