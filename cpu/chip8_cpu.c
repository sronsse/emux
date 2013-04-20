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
#include <memory.h>
#include <video.h>

#define NUM_REGISTERS		16
#define STACK_SIZE		16
#define START_ADDRESS		0x200

#define CPU_CLOCK_RATE		840
#define COUNTERS_CLOCK_RATE	60
#define DRAW_CLOCK_RATE		60

#define SCREEN_WIDTH		64
#define SCREEN_HEIGHT		32
#define CHAR_SIZE		5
#define NUM_PIXELS_PER_BYTE	8

#define SAMPLING_FREQ		11025
#define AUDIO_FORMAT		AUDIO_FORMAT_S16
#define NUM_CHANNELS		1
#define NUM_SAMPLES		512
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
	struct clock cpu_clock;
	struct clock counters_clock;
	struct clock draw_clock;
	float audio_time;
	video_surface_t *surface;
	struct input_config input_config;
	bool keys[NUM_KEYS];
};

static bool chip8_init(struct cpu_instance *instance);
static void chip8_tick(clock_data_t *data);
static void chip8_update_counters(clock_data_t *data);
static void chip8_draw(clock_data_t *data);
static void chip8_mix(audio_data_t *data, void *buffer, int len);
static void chip8_event(int id, struct input_state *state, input_data_t *data);
static void chip8_deinit(struct cpu_instance *instance);
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

void CLS(struct chip8 *chip8)
{
	uint8_t x, y;
	uint32_t black = video_map_rgb(chip8->surface, 0, 0, 0);
	for (x = 0; x < SCREEN_WIDTH; x++)
		for (y = 0; y < SCREEN_HEIGHT; y++)
			video_set_pixel(chip8->surface, x, y, black);
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
	struct surface *s = chip8->surface;
	uint8_t i, j, x, y, b, src, VF = 0;
	uint32_t black = video_map_rgb(chip8->surface, 0, 0, 0);
	uint32_t white = video_map_rgb(chip8->surface, 255, 255, 255);
	bool pixel;

	for (i = 0; i < chip8->opcode.n; i++) {
		b = memory_readb(chip8->I + i);
		y = (chip8->V[chip8->opcode.y] + i) % SCREEN_HEIGHT;
		for (j = 0; j < NUM_PIXELS_PER_BYTE; j++) {
			x = (chip8->V[chip8->opcode.x] + j) % SCREEN_WIDTH;
			src = b >> (NUM_PIXELS_PER_BYTE - j - 1) & 0x01;
			pixel = (video_get_pixel(s, x, y) == white) ^ src;
			video_set_pixel(s, x, y, pixel ? white : black);
			if (src && (video_get_pixel(s, x, y) == black))
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
	memory_writeb(chip8->V[chip8->opcode.x] / 100, chip8->I);
	memory_writeb((chip8->V[chip8->opcode.x] / 10) % 10, chip8->I + 1);
	memory_writeb(chip8->V[chip8->opcode.x] % 10, chip8->I + 2);
}

void LD_cI_Vx(struct chip8 *chip8)
{
	int i;
	for (i = 0; i <= chip8->opcode.x; i++)
		memory_writeb(chip8->V[i], chip8->I + i);
}

void LD_Vx_cI(struct chip8 *chip8)
{
	int i;
	for (i = 0; i <= chip8->opcode.x; i++)
		chip8->V[i] = memory_readb(chip8->I + i);
}

void opcode_0(struct chip8 *chip8)
{
	switch (chip8->opcode.kk) {
	case 0xE0:
		return CLS(chip8);
	case 0xEE:
		return RET(chip8);
	default:
		fprintf(stderr, "chip8: unknown opcode (%04x)!\n",
			chip8->opcode.raw);
		break;
	}
}

void opcode_8(struct chip8 *chip8)
{
	switch (chip8->opcode.n) {
	case 0x00:
		return LD_Vx_Vy(chip8);
	case 0x01:
		return OR_Vx_Vy(chip8);
	case 0x02:
		return AND_Vx_Vy(chip8);
	case 0x03:
		return XOR_Vx_Vy(chip8);
	case 0x04:
		return ADD_Vx_Vy(chip8);
	case 0x05:
		return SUB_Vx_Vy(chip8);
	case 0x06:
		return SHR_Vx(chip8);
	case 0x07:
		return SUBN_Vx_Vy(chip8);
	case 0x0E:
		return SHL_Vx(chip8);
	default:
		fprintf(stderr, "chip8: unknown opcode (%04x)!\n",
			chip8->opcode.raw);
		break;
	}
}

void opcode_E(struct chip8 *chip8)
{
	switch (chip8->opcode.kk) {
	case 0x9E:
		return SKP_Vx(chip8);
	case 0xA1:
		return SKNP_Vx(chip8);
	default:
		fprintf(stderr, "chip8: unknown opcode (%04x)!\n",
			chip8->opcode.raw);
		break;
	}
}

void opcode_F(struct chip8 *chip8)
{
	switch (chip8->opcode.kk) {
	case 0x07:
		return LD_Vx_DT(chip8);
	case 0x0A:
		return LD_Vx_K(chip8);
	case 0x15:
		return LD_DT_Vx(chip8);
	case 0x18:
		return LD_ST_Vx(chip8);
	case 0x1E:
		return ADD_I_Vx(chip8);
	case 0x29:
		return LD_F_Vx(chip8);
	case 0x33:
		return LD_B_Vx(chip8);
	case 0x55:
		return LD_cI_Vx(chip8);
	case 0x65:
		return LD_Vx_cI(chip8);
	default:
		fprintf(stderr, "chip8: unknown opcode (%04x)!\n",
			chip8->opcode.raw);
		break;
	}
}

bool chip8_init(struct cpu_instance *instance)
{
	struct chip8 *chip8;
	struct audio_specs audio_specs;
	struct input_config *input_config;
	int i;

	/* Allocate chip8 structure and set private data */
	chip8 = malloc(sizeof(struct chip8));
	instance->priv_data = chip8;

	/* Initialize audio frontend */
	audio_specs.freq = SAMPLING_FREQ;
	audio_specs.format = AUDIO_FORMAT;
	audio_specs.channels = NUM_CHANNELS;
	audio_specs.samples = NUM_SAMPLES;
	audio_specs.mix = chip8_mix;
	audio_specs.data = chip8;
	if (!audio_init(&audio_specs)) {
		free(chip8);
		return false;
	}

	/* Initialize video frontend */
	if (!video_init(SCREEN_WIDTH, SCREEN_HEIGHT)) {
		free(chip8);
		audio_deinit();
		return false;
	}

	/* Initialize input configuration */
	input_config = &chip8->input_config;
	input_config->events = malloc(NUM_KEYS * sizeof(struct input_event));
	for (i = 0; i < NUM_KEYS; i++) {
		input_config->events[i].type = EVENT_KEYBOARD;
		input_config->events[i].keyboard.key = 'a' + i;
	}
	input_config->num_events = NUM_KEYS;
	input_config->callback = chip8_event;
	input_config->data = chip8;
	memset(chip8->keys, 0, NUM_KEYS * sizeof(bool));
	input_register(input_config);

	/* Initialize registers */
	memset(chip8->V, 0, NUM_REGISTERS);
	chip8->I = 0;
	chip8->PC = START_ADDRESS;
	chip8->SP = 0;
	chip8->DT = 0;
	chip8->ST = 0;

	/* Initialize audio time */
	chip8->audio_time = 0.0f;

	/* Initialize video surface */
	chip8->surface = video_create_surface(SCREEN_WIDTH, SCREEN_HEIGHT);

	/* Add CPU clock */
	chip8->cpu_clock.rate = CPU_CLOCK_RATE;
	chip8->cpu_clock.data = chip8;
	chip8->cpu_clock.tick = chip8_tick;
	clock_add(&chip8->cpu_clock);

	/* Add counters clock */
	chip8->counters_clock.rate = COUNTERS_CLOCK_RATE;
	chip8->counters_clock.data = chip8;
	chip8->counters_clock.tick = chip8_update_counters;
	clock_add(&chip8->counters_clock);

	/* Add draw clock */
	chip8->draw_clock.rate = DRAW_CLOCK_RATE;
	chip8->draw_clock.data = chip8;
	chip8->draw_clock.tick = chip8_draw;
	clock_add(&chip8->draw_clock);

	return true;
}

void chip8_tick(clock_data_t *data)
{
	struct chip8 *chip8 = data;

	/* Fetch opcode */
	uint8_t o1 = memory_readb(chip8->PC++);
	uint8_t o2 = memory_readb(chip8->PC++);
	chip8->opcode.raw = (o1 << 8) | o2;

	/* Execute opcode */
	switch (chip8->opcode.main) {
	case 0x00:
		return opcode_0(chip8);
	case 0x01:
		return JP_addr(chip8);
	case 0x02:
		return CALL_addr(chip8);
	case 0x03:
		return SE_Vx_byte(chip8);
	case 0x04:
		return SNE_Vx_byte(chip8);
	case 0x05:
		return SE_Vx_Vy(chip8);
	case 0x06:
		return LD_Vx_byte(chip8);
	case 0x07:
		return ADD_Vx_byte(chip8);
	case 0x08:
		return opcode_8(chip8);
	case 0x09:
		return SNE_Vx_Vy(chip8);
	case 0x0A:
		return LD_I_addr(chip8);
	case 0x0B:
		return JP_V0_addr(chip8);
	case 0x0C:
		return RND_Vx_byte(chip8);
	case 0x0D:
		return DRW_Vx_Vy_nibble(chip8);
	case 0x0E:
		return opcode_E(chip8);
	case 0x0F:
		return opcode_F(chip8);
	default:
		fprintf(stderr, "chip8: unknown opcode (%04x)!\n",
			chip8->opcode.raw);
		break;
	}
}

void chip8_update_counters(clock_data_t *data)
{
	struct chip8 *chip8 = data;

	/* Update DT and ST counters */
	if (chip8->DT > 0)
		chip8->DT--;
	if (chip8->ST > 0)
		chip8->ST--;

	/* Beep if ST is non-zero */
	if (chip8->ST > 0)
		audio_start();
	else
		audio_stop();
}

void chip8_draw(clock_data_t *data)
{
	struct chip8 *chip8 = data;

	/* Blit surface and update screen */
	video_blit_surface(chip8->surface);
	video_update();
}

void chip8_mix(audio_data_t *data, void *buffer, int len)
{
	struct chip8 *chip8 = data;
	int16_t *b = buffer;
	float sample;
	unsigned int i;

	/* Fill audio buffer */
	for (i = 0; i < len / sizeof(int16_t); i++) {
		/* Compute sine wave sample of desired frequency */
		sample = sinf(2 * M_PI * BEEP_FREQ * chip8->audio_time);
		b[i] = SHRT_MAX * sample;

		/* Increment time */
		chip8->audio_time += 1.0f / SAMPLING_FREQ;
		if (BEEP_FREQ * chip8->audio_time > 1.0f / BEEP_FREQ)
			chip8->audio_time -= 1.0f / BEEP_FREQ;
	}
}

static void chip8_event(int id, struct input_state *state, input_data_t *data)
{
	struct chip8 *chip8 = data;
	chip8->keys[id] = state->active;
}

void chip8_deinit(struct cpu_instance *instance)
{
	struct chip8 *chip8 = instance->priv_data;
	input_unregister(&chip8->input_config);
	free(chip8->input_config.events);
	video_free_surface(chip8->surface);
	video_deinit();
	audio_deinit();
	free(chip8);
}

CPU_START(chip8)
	.init = chip8_init,
	.deinit = chip8_deinit
CPU_END

