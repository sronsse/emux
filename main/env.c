#include <cmdline.h>

/* Command-line parameters */
static char *data_path;
PARAM(data_path, string, NULL, NULL, NULL)
static char *system_path = "";
PARAM(system_path, string, "system-dir", NULL, "Path to system directory")
static char *config_path = "";
PARAM(config_path, string, "config-dir", NULL, "Path to config directory")
static char *save_path = "";
PARAM(save_path, string, "save-dir", NULL, "Path to save directory")

char *env_get_data_path()
{
	return data_path;
}

char *env_get_system_path()
{
	return system_path;
}

char *env_get_config_path()
{
	return config_path;
}

char *env_get_save_path()
{
	return save_path;
}

