#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct cmdline {
	int argc;
	char **argv;
};

static bool cmdline_parse(char *long_name,
	bool has_arg,
	char **arg);

static struct cmdline cmdline;

void cmdline_init(int argc, char *argv[])
{
	/* Save command line for later use */
	cmdline.argc = argc;
	cmdline.argv = argv;
}

bool cmdline_parse(char *long_name, bool has_arg, char **arg)
{
	int has_argument = has_arg ? required_argument : no_argument;
	struct option long_options[] = {
		{ long_name, has_argument, NULL, 'o' },
		{ 0, 0, 0, 0 }
	};
	int c;

	/* Reset option index and make sure errors don't get printed out */
	optind = 1;
	opterr = 0;

	/* Parse option until we either find requested one or none is left */
	do {
		c = getopt_long_only(cmdline.argc,
			cmdline.argv,
			"",
			long_options,
			NULL);

		/* Check if option is found */
		if (c == 'o') {
			if (has_arg)
				*arg = optarg;
			return true;
		}
	} while (c != -1);

	return false;
}

bool cmdline_parse_bool(char *long_name, bool *arg)
{
	bool b;

	b = cmdline_parse(long_name, false, NULL);

	if (arg)
		*arg = b;
	return b;
}

bool cmdline_parse_int(char *long_name, int *arg)
{
	char *str;
	char *end;
	int i;

	if (!cmdline_parse(long_name, true, &str))
		return false;

	i = strtol(str, &end, 10);
	if (*end)
		return false;

	if (arg)
		*arg = i;
	return true;
}

bool cmdline_parse_string(char *long_name, char **arg)
{
	char *str;

	if (!cmdline_parse(long_name, true, &str))
		return false;

	if (arg)
		*arg = str;
	return true;
}

