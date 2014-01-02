#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <config.h>
#include <libretro.h>

static uint16_t *frame_buf;
static struct retro_log_callback logging;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
	(void)level;
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

void retro_init(void)
{
	frame_buf = calloc(320 * 240, sizeof(uint16_t));
}

void retro_deinit(void)
{
	free(frame_buf);
	frame_buf = NULL;
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
	info->timing = (struct retro_system_timing) {
		.fps = 60.0,
		.sample_rate = 30000.0
	};

	info->geometry = (struct retro_game_geometry) {
		.base_width = 320,
		.base_height = 240,
		.max_width = 320,
		.max_height = 240,
		.aspect_ratio = 4.0 / 3.0
	};
}

void retro_set_environment(retro_environment_t cb)
{
	bool no_rom = true;

	environ_cb = cb;
	cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);

	if (!cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
		logging.log = fallback_log;
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

void retro_set_video_refresh(retro_video_refresh_t cb)
{
	video_cb = cb;
}

void retro_reset(void)
{
}

void retro_run(void)
{
	int x;
	int y;

	input_poll_cb();

	for (y = 0; y < 240; y++)
		for (x = 0; x < 320; x++)
			frame_buf[y * 320 + x] = 0;
	video_cb(frame_buf, 320, 240, 320 << 1);
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

