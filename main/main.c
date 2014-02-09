#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <cmdline.h>
#include <config.h>
#include <env.h>
#include <log.h>
#include <machine.h>

#if (defined(CONFIG_AUDIO_SDL) || defined(CONFIG_VIDEO_SDL)) && \
	defined(__APPLE__)
#include <SDL.h>
#endif

/* Command-line parameter */
static bool help = false;
PARAM(help, bool, "help", NULL, "Display this help and exit")

int main(int argc, char *argv[])
{
	/* Initialize random seed */
	srand(time(NULL));

	/* Initialize command line and fill all parameters */
	cmdline_init(argc, argv);

	/* Check if user requires help */
	if (help) {
		cmdline_print_usage(false);
		return 0;
	}

	/* Validate that a path was given */
	if (!env_get_data_path()) {
		LOG_E("No path specified!\n");
		goto err;
	}

	/* Initialize, run, and deinitialize machine */
	if (!machine_init())
		goto err;
	machine_run();
	machine_deinit();

	return 0;
err:
	cmdline_print_usage(true);
	return 1;
}

