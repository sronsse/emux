#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <audio.h>
#include <clock.h>
#include <cpu.h>
#include <input.h>
#ifdef __LIBRETRO__
#include <libretro.h>
#endif
#include <log.h>
#include <memory.h>
#include <util.h>
#include <video.h>

#define NUM_REGISTERS		16
#define STACK_SIZE		16
#define START_ADDRESS		0x200

#define CPU_CLOCK_RATE		840.0f
#define COUNTERS_CLOCK_RATE	60.0f
#define DRAW_CLOCK_RATE		60.0f

#define SCREEN_WIDTH		64
#define SCREEN_HEIGHT		32
#define CHAR_SIZE		5
#define NUM_PIXELS_PER_BYTE	8

#define SAMPLING_FREQ		48000
#define AUDIO_FORMAT		AUDIO_FORMAT_S16
#define BEEP_FREQ		440

#define NUM_KEYS		16

union opcode {
	uint16_t raw;
	struct {
		uint16_t nnn:12;
		uint16_t main:4;
	};
	uint16_t kk:8;
	struct {
		uint16_t n:4;
		uint16_t y:4;
		uint16_t x:4;
	};
};

struct chip8 {
	uint8_t V[NUM_REGISTERS];
	uint16_t I;
	uint16_t PC;
	uint8_t SP;
	uint8_t DT;
	uint8_t ST;
	union opcode opcode;
	uint16_t stack[STACK_SIZE];
	int bus_id;
	struct clock cpu_clock;
	struct clock counters_clock;
	struct clock draw_clock;
	int16_t *audio_buffer;
	int audio_samples;
	float audio_time;
	struct input_config input_config;
	bool keys[NUM_KEYS];
};

static bool chip8_init(struct cpu_instance *instance);
static void chip8_reset(struct cpu_instance *instance);
static void chip8_deinit(struct cpu_instance *instance);
static void chip8_tick(struct chip8 *chip8);
static void chip8_gen_audio(struct chip8 *chip8);
static void chip8_update_counters(struct chip8 *chip8);
static void chip8_draw(clock_data_t *data);
static void chip8_event(int id, enum input_type type, struct chip8 *chip8);
static inline void CLS(struct chip8 *chip8);
static inline void RET(struct chip8 *chip8);
static inline void JP_addr(struct chip8 *chip8);
static inline void CALL_addr(struct chip8 *chip8);
static inline void SE_Vx_byte(struct chip8 *chip8);
static inline void SNE_Vx_byte(struct chip8 *chip8);
static inline void SE_Vx_Vy(struct chip8 *chip8);
static inline void LD_Vx_byte(struct chip8 *chip8);
static inline void ADD_Vx_byte(struct chip8 *chip8);
static inline void LD_Vx_Vy(struct chip8 *chip8);
static inline void OR_Vx_Vy(struct chip8 *chip8);
static inline void AND_Vx_Vy(struct chip8 *chip8);
static inline void XOR_Vx_Vy(struct chip8 *chip8);
static inline void ADD_Vx_Vy(struct chip8 *chip8);
static inline void SUB_Vx_Vy(struct chip8 *chip8);
static inline void SHR_Vx(struct chip8 *chip8);
static inline void SUBN_Vx_Vy(struct chip8 *chip8);
static inline void SHL_Vx(struct chip8 *chip8);
static inline void SNE_Vx_Vy(struct chip8 *chip8);
static inline void LD_I_addr(struct chip8 *chip8);
static inline void JP_V0_addr(struct chip8 *chip8);
static inline void RND_Vx_byte(struct chip8 *chip8);
static inline void DRW_Vx_Vy_nibble(struct chip8 *chip8);
static inline void SKP_Vx(struct chip8 *chip8);
static inline void SKNP_Vx(struct chip8 *chip8);
static inline void LD_Vx_DT(struct chip8 *chip8);
static inline void LD_Vx_K(struct chip8 *chip8);
static inline void LD_DT_Vx(struct chip8 *chip8);
static inline void LD_ST_Vx(struct chip8 *chip8);
static inline void ADD_I_Vx(struct chip8 *chip8);
static inline void LD_F_Vx(struct chip8 *chip8);
static inline void LD_B_Vx(struct chip8 *chip8);
static inline void LD_cI_Vx(struct chip8 *chip8);
static inline void LD_Vx_cI(struct chip8 *chip8);
static void opcode_0(struct chip8 *chip8);
static void opcode_8(struct chip8 *chip8);
static void opcode_E(struct chip8 *chip8);
static void opcode_F(struct chip8 *chip8);

static struct input_desc input_descs[] = {
#ifndef __LIBRETRO__
	{ "Key 0", DEVICE_KEYBOARD, KEY_a },
	{ "Key 1", DEVICE_KEYBOARD, KEY_b },
	{ "Key 2", DEVICE_KEYBOARD, KEY_c },
	{ "Key 3", DEVICE_KEYBOARD, KEY_d },
	{ "Key 4", DEVICE_KEYBOARD, KEY_e },
	{ "Key 5", DEVICE_KEYBOARD, KEY_f },
	{ "Key 6", DEVICE_KEYBOARD, KEY_g },
	{ "Key 7", DEVICE_KEYBOARD, KEY_h },
	{ "Key 8", DEVICE_KEYBOARD, KEY_i },
	{ "Key 9", DEVICE_KEYBOARD, KEY_j },
	{ "Key A", DEVICE_KEYBOARD, KEY_k },
	{ "Key B", DEVICE_KEYBOARD, KEY_l },
	{ "Key C", DEVICE_KEYBOARD, KEY_m },
	{ "Key D", DEVICE_KEYBOARD, KEY_n },
	{ "Key E", DEVICE_KEYBOARD, KEY_o },
	{ "Key F", DEVICE_KEYBOARD, KEY_p }
#else
	{ "Key 0", RETRO_DEVICE_KEYBOARD, KEY_a },
	{ "Key 1", RETRO_DEVICE_KEYBOARD, KEY_b },
	{ "Key 2", RETRO_DEVICE_KEYBOARD, KEY_c },
	{ "Key 3", RETRO_DEVICE_KEYBOARD, KEY_d },
	{ "Key 4", RETRO_DEVICE_KEYBOARD, KEY_e },
	{ "Key 5", RETRO_DEVICE_KEYBOARD, KEY_f },
	{ "Key 6", RETRO_DEVICE_KEYBOARD, KEY_g },
	{ "Key 7", RETRO_DEVICE_KEYBOARD, KEY_h },
	{ "Key 8", RETRO_DEVICE_KEYBOARD, KEY_i },
	{ "Key 9", RETRO_DEVICE_KEYBOARD, KEY_j },
	{ "Key A", RETRO_DEVICE_KEYBOARD, KEY_k },
	{ "Key B", RETRO_DEVICE_KEYBOARD, KEY_l },
	{ "Key C", RETRO_DEVICE_KEYBOARD, KEY_m },
	{ "Key D", RETRO_DEVICE_KEYBOARD, KEY_n },
	{ "Key E", RETRO_DEVICE_KEYBOARD, KEY_o },
	{ "Key F", RETRO_DEVICE_KEYBOARD, KEY_p }
#endif
};

void CLS(struct chip8 *UNUSED(chip8))
{
	uint8_t x, y;
	struct color black = { 0, 0, 0 };

	video_lock();
	for (x = 0; x < SCREEN_WIDTH; x++)
		for (y = 0; y < SCREEN_HEIGHT; y++)
			video_set_pixel(x, y, black);
	video_unlock();
}

void RET(struct chip8 *chip8)
{
	chip8->PC = chip8->stack[--chip8->SP];
}

void JP_addr(struct chip8 *chip8)
{
	chip8->PC = chip8->opcode.nnn;
}

void CALL_addr(struct chip8 *chip8)
{
	chip8->stack[chip8->SP++] = chip8->PC;
	chip8->PC = chip8->opcode.nnn;
}

void SE_Vx_byte(struct chip8 *chip8)
{
	if (chip8->V[chip8->opcode.x] == chip8->opcode.kk)
		chip8->PC += 2;
}

void SNE_Vx_byte(struct chip8 *chip8)
{
	if (chip8->V[chip8->opcode.x] != chip8->opcode.kk)
		chip8->PC += 2;
}

void SE_Vx_Vy(struct chip8 *chip8)
{
	if (chip8->V[chip8->opcode.x] == chip8->V[chip8->opcode.y])
		chip8->PC += 2;
}

void LD_Vx_byte(struct chip8 *chip8)
{
	chip8->V[chip8->opcode.x] = chip8->opcode.kk;
}

void ADD_Vx_byte(struct chip8 *chip8)
{
	chip8->V[chip8->opcode.x] += chip8->opcode.kk;
	chip8->V[chip8->opcode.x] &= 0xFF;
}

void LD_Vx_Vy(struct chip8 *chip8)
{
	chip8->V[chip8->opcode.x] = chip8->V[chip8->opcode.y];
}

void OR_Vx_Vy(struct chip8 *chip8)
{
	chip8->V[chip8->opcode.x] |= chip8->V[chip8->opcode.y];
}

void AND_Vx_Vy(struct chip8 *chip8)
{
	chip8->V[chip8->opcode.x] &= chip8->V[chip8->opcode.y];
}

void XOR_Vx_Vy(struct chip8 *chip8)
{
	chip8->V[chip8->opcode.x] ^= chip8->V[chip8->opcode.y];
}

void ADD_Vx_Vy(struct chip8 *chip8)
{
	uint16_t result = chip8->V[chip8->opcode.x] + chip8->V[chip8->opcode.y];
	chip8->V[chip8->opcode.x] = result & 0xFF;
	chip8->V[0x0F] = (result > 0xFF);
}

void SUB_Vx_Vy(struct chip8 *chip8)
{
	uint8_t x = chip8->V[chip8->opcode.x];
	uint8_t y = chip8->V[chip8->opcode.y];
	chip8->V[0x0F] = (x > y);
	chip8->V[chip8->opcode.x] -= chip8->V[chip8->opcode.y];
	chip8->V[chip8->opcode.x] &= 0xFF;
}

void SHR_Vx(struct chip8 *chip8)
{
	chip8->V[0x0F] = ((chip8->V[chip8->opcode.x] & 0x01) != 0);
	chip8->V[chip8->opcode.x] >>= 1;
}

void SUBN_Vx_Vy(struct chip8 *chip8)
{
	uint8_t x = chip8->V[chip8->opcode.x];
	uint8_t y = chip8->V[chip8->opcode.y];
	chip8->V[0x0F] = (x > y);
	chip8->V[chip8->opcode.x] = chip8->V[chip8->opcode.y] -
		chip8->V[chip8->opcode.x];
	chip8->V[chip8->opcode.x] &= 0xFF;
}

void SHL_Vx(struct chip8 *chip8)
{
	chip8->V[0x0F] = ((chip8->V[chip8->opcode.x] & 0x80) != 0);
	chip8->V[chip8->opcode.x] <<= 1;
	chip8->V[chip8->opcode.x] &= 0xFF;
}

void SNE_Vx_Vy(struct chip8 *chip8)
{
	if (chip8->V[chip8->opcode.x] != chip8->V[chip8->opcode.y])
		chip8->PC += 2;
}

void LD_I_addr(struct chip8 *chip8)
{
	chip8->I = chip8->opcode.nnn;
}

void JP_V0_addr(struct chip8 *chip8)
{
	chip8->PC = chip8->opcode.nnn + chip8->V[0];
}

void RND_Vx_byte(struct chip8 *chip8)
{
	chip8->V[chip8->opcode.x] = (rand() % 256) & chip8->opcode.kk;
}

void DRW_Vx_Vy_nibble(struct chip8 *chip8)
{
	uint8_t i, j, x, y, b, src, VF = 0;
	struct color black = { 0, 0, 0 };
	struct color white = { 255, 255, 255 };
	struct color c;
	bool pixel;

	for (i = 0; i < chip8->opcode.n; i++) {
		b = memory_readb(chip8->bus_id, chip8->I + i);
		y = (chip8->V[chip8->opcode.y] + i) % SCREEN_HEIGHT;
		for (j = 0; j < NUM_PIXELS_PER_BYTE; j++) {
			x = (chip8->V[chip8->opcode.x] + j) % SCREEN_WIDTH;
			src = b >> (NUM_PIXELS_PER_BYTE - j - 1) & 0x01;
			c = video_get_pixel(x, y);
			pixel = (c.r == white.r) ^ src;
			video_set_pixel(x, y, pixel ? white : black);
			if (src && !pixel)
				VF = 1;
		}
	}
	chip8->V[0x0F] = VF;
}

void SKP_Vx(struct chip8 *chip8)
{
	if (chip8->keys[chip8->V[chip8->opcode.x]])
		chip8->PC += 2;
}

void SKNP_Vx(struct chip8 *chip8)
{
	if (!chip8->keys[chip8->V[chip8->opcode.x]])
		chip8->PC += 2;
}

void LD_Vx_DT(struct chip8 *chip8)
{
	chip8->V[chip8->opcode.x] = chip8->DT;
}

void LD_Vx_K(struct chip8 *chip8)
{
	int i;

	/* Loop through key states and store pressed key if needed */
	for (i = 0; i < NUM_KEYS; i++)
		if (chip8->keys[i]) {
			chip8->V[chip8->opcode.x] = i;
			return;
		}

	/* Key not found, so wait */
	chip8->PC -= 2;
}

void LD_DT_Vx(struct chip8 *chip8)
{
	chip8->DT = chip8->V[chip8->opcode.x];
}

void LD_ST_Vx(struct chip8 *chip8)
{
	chip8->ST = chip8->V[chip8->opcode.x];
}

void ADD_I_Vx(struct chip8 *chip8)
{
	chip8->I += chip8->V[chip8->opcode.x];
}

void LD_F_Vx(struct chip8 *chip8)
{
	chip8->I = (chip8->V[chip8->opcode.x] & 0x0F) * CHAR_SIZE;
}

void LD_B_Vx(struct chip8 *chip8)
{
	memory_writeb(chip8->bus_id, chip8->V[chip8->opcode.x] / 100,
		chip8->I);
	memory_writeb(chip8->bus_id, (chip8->V[chip8->opcode.x] / 10) % 10,
		chip8->I + 1);
	memory_writeb(chip8->bus_id, chip8->V[chip8->opcode.x] % 10,
		chip8->I + 2);
}

void LD_cI_Vx(struct chip8 *chip8)
{
	int i;
	for (i = 0; i <= chip8->opcode.x; i++)
		memory_writeb(chip8->bus_id, chip8->V[i], chip8->I + i);
}

void LD_Vx_cI(struct chip8 *chip8)
{
	int i;
	for (i = 0; i <= chip8->opcode.x; i++)
		chip8->V[i] = memory_readb(chip8->bus_id, chip8->I + i);
}

void opcode_0(struct chip8 *chip8)
{
	switch (chip8->opcode.kk) {
	case 0xE0:
		CLS(chip8);
		break;
	case 0xEE:
		RET(chip8);
		break;
	default:
		LOG_W("chip8: unknown opcode (%04x)!\n", chip8->opcode.raw);
		break;
	}
}

void opcode_8(struct chip8 *chip8)
{
	switch (chip8->opcode.n) {
	case 0x00:
		LD_Vx_Vy(chip8);
		break;
	case 0x01:
		OR_Vx_Vy(chip8);
		break;
	case 0x02:
		AND_Vx_Vy(chip8);
		break;
	case 0x03:
		XOR_Vx_Vy(chip8);
		break;
	case 0x04:
		ADD_Vx_Vy(chip8);
		break;
	case 0x05:
		SUB_Vx_Vy(chip8);
		break;
	case 0x06:
		SHR_Vx(chip8);
		break;
	case 0x07:
		SUBN_Vx_Vy(chip8);
		break;
	case 0x0E:
		SHL_Vx(chip8);
		break;
	default:
		LOG_W("chip8: unknown opcode (%04x)!\n", chip8->opcode.raw);
		break;
	}
}

void opcode_E(struct chip8 *chip8)
{
	switch (chip8->opcode.kk) {
	case 0x9E:
		SKP_Vx(chip8);
		break;
	case 0xA1:
		SKNP_Vx(chip8);
		break;
	default:
		LOG_W("chip8: unknown opcode (%04x)!\n", chip8->opcode.raw);
		break;
	}
}

void opcode_F(struct chip8 *chip8)
{
	switch (chip8->opcode.kk) {
	case 0x07:
		LD_Vx_DT(chip8);
		break;
	case 0x0A:
		LD_Vx_K(chip8);
		break;
	case 0x15:
		LD_DT_Vx(chip8);
		break;
	case 0x18:
		LD_ST_Vx(chip8);
		break;
	case 0x1E:
		ADD_I_Vx(chip8);
		break;
	case 0x29:
		LD_F_Vx(chip8);
		break;
	case 0x33:
		LD_B_Vx(chip8);
		break;
	case 0x55:
		LD_cI_Vx(chip8);
		break;
	case 0x65:
		LD_Vx_cI(chip8);
		break;
	default:
		LOG_W("chip8: unknown opcode (%04x)!\n", chip8->opcode.raw);
		break;
	}
}

void chip8_tick(struct chip8 *chip8)
{
	/* Fetch opcode */
	uint8_t o1 = memory_readb(chip8->bus_id, chip8->PC++);
	uint8_t o2 = memory_readb(chip8->bus_id, chip8->PC++);
	chip8->opcode.raw = (o1 << 8) | o2;

	/* Execute opcode */
	switch (chip8->opcode.main) {
	case 0x00:
		opcode_0(chip8);
		break;
	case 0x01:
		JP_addr(chip8);
		break;
	case 0x02:
		CALL_addr(chip8);
		break;
	case 0x03:
		SE_Vx_byte(chip8);
		break;
	case 0x04:
		SNE_Vx_byte(chip8);
		break;
	case 0x05:
		SE_Vx_Vy(chip8);
		break;
	case 0x06:
		LD_Vx_byte(chip8);
		break;
	case 0x07:
		ADD_Vx_byte(chip8);
		break;
	case 0x08:
		opcode_8(chip8);
		break;
	case 0x09:
		SNE_Vx_Vy(chip8);
		break;
	case 0x0A:
		LD_I_addr(chip8);
		break;
	case 0x0B:
		JP_V0_addr(chip8);
		break;
	case 0x0C:
		RND_Vx_byte(chip8);
		break;
	case 0x0D:
		DRW_Vx_Vy_nibble(chip8);
		break;
	case 0x0E:
		opcode_E(chip8);
		break;
	case 0x0F:
		opcode_F(chip8);
		break;
	default:
		LOG_W("chip8: unknown opcode (%04x)!\n", chip8->opcode.raw);
		break;
	}

	/* Report cycle consumption */
	clock_consume(1);
}

void chip8_gen_audio(struct chip8 *chip8)
{
	int buffer_size = chip8->audio_samples * sizeof(int16_t);
	float sample;
	int i;

	/* Zero-out buffer */
	memset(chip8->audio_buffer, 0, buffer_size);

	/* Beep if ST is non-zero */
	if (chip8->ST > 0)
		for (i = 0; i < chip8->audio_samples; i++) {
			/* Compute sine wave sample of desired frequency */
			sample = sinf(2 * M_PI * BEEP_FREQ * chip8->audio_time);
			chip8->audio_buffer[i] = SHRT_MAX * sample;

			/* Increment time */
			chip8->audio_time += 1.0f / SAMPLING_FREQ;
			if (BEEP_FREQ * chip8->audio_time > 1.0f / BEEP_FREQ)
				chip8->audio_time -= 1.0f / BEEP_FREQ;
		}

	/* Enqueue audio buffer */
	audio_enqueue((uint8_t *)chip8->audio_buffer, chip8->audio_samples);
}

void chip8_update_counters(struct chip8 *chip8)
{
	/* Generate audio sample based on sound counter */
	chip8_gen_audio(chip8);

	/* Update DT and ST counters */
	if (chip8->DT > 0)
		chip8->DT--;
	if (chip8->ST > 0)
		chip8->ST--;

	/* Report cycle consumption */
	clock_consume(1);
}

void chip8_draw(clock_data_t *UNUSED(data))
{
	video_update();

	/* Report cycle consumption */
	clock_consume(1);
}

static void chip8_event(int id, enum input_type type, struct chip8 *chip8)
{
	chip8->keys[id] = (type == EVENT_BUTTON_DOWN);
}

bool chip8_init(struct cpu_instance *instance)
{
	struct chip8 *chip8;
	struct audio_specs audio_specs;
	struct video_specs video_specs;
	struct input_config *input_config;

	/* Allocate chip8 structure and set private data */
	chip8 = calloc(1, sizeof(struct chip8));
	instance->priv_data = chip8;

	/* Initialize audio frontend */
	audio_specs.freq = SAMPLING_FREQ;
	audio_specs.format = AUDIO_FORMAT;
	audio_specs.channels = 1;
	if (!audio_init(&audio_specs)) {
		free(chip8);
		return false;
	}

	/* Initialize video frontend */
	video_specs.width = SCREEN_WIDTH;
	video_specs.height = SCREEN_HEIGHT;
	video_specs.fps = DRAW_CLOCK_RATE;
	if (!video_init(&video_specs)) {
		free(chip8);
		audio_deinit();
		return false;
	}

	/* Initialize input configuration */
	input_config = &chip8->input_config;
	input_config->name = instance->cpu_name;
	input_config->descs = input_descs;
	input_config->num_descs = ARRAY_SIZE(input_descs);
	input_config->data = chip8;
	input_config->callback = (input_cb_t)chip8_event;
	input_register(input_config, true);

	/* Initialize audio data */
	chip8->audio_samples = SAMPLING_FREQ / COUNTERS_CLOCK_RATE;
	chip8->audio_buffer = calloc(chip8->audio_samples, sizeof(int16_t));

	/* Save bus ID for later use */
	chip8->bus_id = instance->bus_id;

	/* Add CPU clock */
	chip8->cpu_clock.rate = CPU_CLOCK_RATE;
	chip8->cpu_clock.data = chip8;
	chip8->cpu_clock.tick = (clock_tick_t)chip8_tick;
	clock_add(&chip8->cpu_clock);

	/* Add counters clock */
	chip8->counters_clock.rate = COUNTERS_CLOCK_RATE;
	chip8->counters_clock.data = chip8;
	chip8->counters_clock.tick = (clock_tick_t)chip8_update_counters;
	clock_add(&chip8->counters_clock);

	/* Add draw clock */
	chip8->draw_clock.rate = DRAW_CLOCK_RATE;
	chip8->draw_clock.tick = chip8_draw;
	clock_add(&chip8->draw_clock);

	return true;
}

void chip8_reset(struct cpu_instance *instance)
{
	struct chip8 *chip8 = instance->priv_data;
	struct color black = { 0, 0, 0 };
	int x;
	int y;

	/* Initialize registers */
	memset(chip8->V, 0, NUM_REGISTERS);
	chip8->I = 0;
	chip8->PC = START_ADDRESS;
	chip8->SP = 0;
	chip8->DT = 0;
	chip8->ST = 0;

	/* Initialize screen */
	for (y = 0; y < SCREEN_HEIGHT; y++)
		for (x = 0; x < SCREEN_WIDTH; x++)
			video_set_pixel(x, y, black);

	/* Initialize input data */
	memset(chip8->keys, 0, NUM_KEYS * sizeof(bool));

	/* Reset audio time */
	chip8->audio_time = 0.0f;

	/* Set initial clock states */
	chip8->cpu_clock.enabled = true;
	chip8->counters_clock.enabled = true;
	chip8->draw_clock.enabled = true;
}

void chip8_deinit(struct cpu_instance *instance)
{
	struct chip8 *chip8 = instance->priv_data;
	input_unregister(&chip8->input_config);
	free(chip8->audio_buffer);
	video_deinit();
	audio_deinit();
	free(chip8);
}

CPU_START(chip8)
	.init = chip8_init,
	.reset = chip8_reset,
	.deinit = chip8_deinit
CPU_END

