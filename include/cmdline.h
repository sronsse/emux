#ifndef _CMDLINE_H
#define _CMDLINE_H

#include <stdbool.h>

void cmdline_init(int argc, char *argv[]);
bool cmdline_parse_bool(char *long_name, bool *arg);
bool cmdline_parse_int(char *long_name, int *arg);
bool cmdline_parse_string(char *long_name, char **string);

#endif

