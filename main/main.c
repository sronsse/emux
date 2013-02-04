#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <config.h>
#include <machine.h>

static void print_usage(char *program, bool error);

void print_usage(char *program, bool error)
{
	fprintf(error ? stderr : stdout,
		"Usage: %s -m <machine> -o <options>\n",
		program);
}

int main(int argc, char *argv[])
{
	char *machine = NULL;
	char *options = NULL;
	int c;
	int i;

	/* Print version and command line */
	fprintf(stdout, "Emux version %s\n", PACKAGE_VERSION);
	fprintf(stdout, "Command line:");
	for (i = 0; i < argc; i++)
		fprintf(stdout, " %s", argv[i]);
	fprintf(stdout, "\n");

	/* Get machine name and options */
	while ((c = getopt(argc, argv, "m:o:")) != -1)
		switch (c) {
		case 'm':
			machine = optarg;
			break;
		case 'o':
			options = optarg;
			break;
		case '?':
			print_usage(argv[0], false);
			return 1;
		default:
			print_usage(argv[0], true);
			return 1;
		}

	/* Validate arguments were passed correctly */
	if (!machine || !options || (optind != argc)) {
		print_usage(argv[0], true);
		return 1;
	}

	if (!machine_init(machine))
		return 1;

	/* Deinitialize machine */
	machine_deinit();

	return 0;
}

