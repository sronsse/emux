#ifndef _SMS_MAPPER_H
#define _SMS_MAPPER_H

#include <memory.h>

struct sms_mapper_mach_data {
	char *bios_path;
	char *cart_path;
};

struct cart_mapper_mach_data {
	char *cart_path;
	struct region *cart_region;
};

#endif

