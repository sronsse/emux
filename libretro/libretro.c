#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <cmdline.h>
#include <config.h>
#include <libretro.h>
#include <log.h>
#include <video.h>

/* Retro video frontend functions */
void retro_video_fill_timing(struct retro_system_timing *timing);
void retro_video_fill_geometry(struct retro_game_geometry *geometry);
bool retro_video_updated();

retro_environment_t retro_environment_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

void retro_init(void)
{
	/* Set retro as the video frontend */
	cmdline_set_param("video", NULL, "retro");

	video_init(320, 240);
}

void retro_deinit(void)
{
	video_deinit();
}

unsigned int retro_api_version(void)
{
	return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned int port, unsigned int device)
{
	(void)port;
	(void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
	memset(info, 0, sizeof(*info));
	info->library_name = PACKAGE_NAME;
	info->library_version = PACKAGE_VERSION;
	info->need_fullpath = false;
	info->valid_extensions = NULL;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
	info->timing.sample_rate = 30000.0;

	/* Let video frontend fill timing and geometry */
	retro_video_fill_timing(&info->timing);
	retro_video_fill_geometry(&info->geometry);
}

void retro_set_environment(retro_environment_t cb)
{
	struct retro_log_callback log_callback;
	bool no_rom = true;

	/* Set retro environment callback */
	retro_environment_cb = cb;

	cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);

	/* Override log callback if supported by frontend */
	if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_callback))
		log_cb = (log_print_t)log_callback.log;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
	audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
	input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
	input_state_cb = cb;
}

void retro_reset(void)
{
}

void retro_run(void)
{
	input_poll_cb();

	/* Run until screen is updated */
	while (!retro_video_updated())
		video_update();
}

bool retro_load_game(const struct retro_game_info *info)
{
	return true;
}

void retro_unload_game(void)
{
}

unsigned int retro_get_region(void)
{
	return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned int type,
	const struct retro_game_info *info, size_t num)
{
	(void)type;
	(void)info;
	(void)num;
	return false;
}

size_t retro_serialize_size(void)
{
	return 0;
}

bool retro_serialize(void *data_, size_t size)
{
	return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
	return false;
}

void *retro_get_memory_data(unsigned int id)
{
	(void)id;
	return NULL;
}

size_t retro_get_memory_size(unsigned int id)
{
	(void)id;
	return 0;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned int index, bool enabled, const char *code)
{
	(void)index;
	(void)enabled;
	(void)code;
}

