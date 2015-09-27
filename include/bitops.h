#ifndef _BITOPS_H
#define _BITOPS_H

#include <stdbool.h>
#include <stdint.h>

#define DEFINE_BITOPS_GET(ext, type) \
	static inline type bitops_get##ext(type *a, type sh, type l) \
	{ \
		return (*a >> sh) & (BIT(l) - 1); \
	}

#define DEFINE_BITOPS_SET(ext, type) \
	static inline void bitops_set##ext(type *a, type sh, type l, type v) \
	{ \
		*a &= ~((BIT(l) - 1) << sh); \
		*a |= (v << sh); \
	}

#define BIT(n) (1 << (n))

/* Define bitops functions */
DEFINE_BITOPS_GET(b, uint8_t)
DEFINE_BITOPS_SET(b, uint8_t)
DEFINE_BITOPS_GET(w, uint16_t)
DEFINE_BITOPS_SET(w, uint16_t)
DEFINE_BITOPS_GET(l, uint32_t)
DEFINE_BITOPS_SET(l, uint32_t)

static inline int bitops_reverse(int i, int length)
{
	uint8_t index;
	int r = 0;
	int b;

	/* Reverse bits */
	for (index = 0; index < length; index++) {
		b = (i >> (length - index - 1)) & 0x01;
		r |= b << index;
	}

	return r;
}

static inline int bitops_ffs(int i)
{
	int bit;

	/* Handle case where no bit is set */
	if (i == 0)
		return 0;

	/* Find position of first bit set within argument */
	for (bit = 1; (i & 0x01) == 0; bit++)
		i >>= 1;

	return bit;
}

static inline bool bitops_parity(int i)
{
	bool parity = false;

	/* Compute input parity */
	while (i) {
		parity = !parity;
		i = i & (i - 1);
	}

	return parity;
}


#endif

