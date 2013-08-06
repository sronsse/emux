#ifndef _BITOPS_H
#define _BITOPS_H

#include <stdint.h>

#define DECLARE_BITOPS_GET(ext, type) \
	type bitops_get##ext(type *a, type shift, type length);

#define DECLARE_BITOPS_SET(ext, type) \
	void bitops_set##ext(type *a, type shift, type length, type v);

#define BIT(n) (1 << (n))

/* Declare BITOPS functions */
DECLARE_BITOPS_GET(b, uint8_t);
DECLARE_BITOPS_SET(b, uint8_t);
DECLARE_BITOPS_GET(w, uint16_t);
DECLARE_BITOPS_SET(w, uint16_t);

#endif

