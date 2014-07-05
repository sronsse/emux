#include <stdint.h>
#include <strings.h>
#include <bitops.h>
#include <config.h>

#ifndef HAVE_FFS
#define ffs __builtin_ffs
#endif

#define DEFINE_BITOPS_GET(ext, type) \
	type bitops_get##ext(type *a, type shift, type length) \
	{ \
		return (*a >> shift) & (BIT(length) - 1); \
	}

#define DEFINE_BITOPS_SET(ext, type) \
	void bitops_set##ext(type *a, type shift, type length, type v) \
	{ \
		*a &= ~((BIT(length) - 1) << shift); \
		*a |= (v << shift); \
	}

/* Define bitops functions */
DEFINE_BITOPS_GET(b, uint8_t)
DEFINE_BITOPS_SET(b, uint8_t)
DEFINE_BITOPS_GET(w, uint16_t)
DEFINE_BITOPS_SET(w, uint16_t)

int bitops_reverse(int i, int length)
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

int bitops_ffs(int i)
{
	return ffs(i);
}

