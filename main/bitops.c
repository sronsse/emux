#include <stdint.h>
#include <strings.h>
#include <bitops.h>

#ifdef _WIN32
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
DEFINE_BITOPS_GET(b, uint8_t);
DEFINE_BITOPS_SET(b, uint8_t);
DEFINE_BITOPS_GET(w, uint16_t);
DEFINE_BITOPS_SET(w, uint16_t);

int bitops_ffs(int i)
{
	return ffs(i);
}

