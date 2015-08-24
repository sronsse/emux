#include <stdlib.h>
#include <audio.h>
#include <bitops.h>
#include <clock.h>
#include <controller.h>
#include <port.h>

/* Controller definitions */
#define INTERNAL_DIVIDER	16
#define NUM_CHANNELS		4
#define NUM_TONE_CHANNELS	3
#define NOISE_CHANNEL		3
#define LATCH_DATA_CMD_TYPE	1
#define DATA_CMD_TYPE		0
#define LATCH_VOLUME_CMD_TYPE	1
#define LATCH_TONE_CMD_TYPE	0
#define MAX_ATTENUATION		0x0F
#define MAX_VOLUME		0xFF
#define WHITE_NOISE		1
#define PERIODIC_NOISE		0
#define TAP_MASK		0x09

union volume_register {
	uint8_t raw;
	struct {
		uint8_t attenuation:4;
		uint8_t reserved:4;
	};
};

union tone_register {
	uint16_t raw;
	struct {
		uint16_t reset_value:10;
		uint16_t reserved:6;
	};
};

union noise_register {
	uint8_t raw;
	struct {
		uint8_t shift_rate:2;
		uint8_t mode:1;
		uint8_t reserved:5;
	};
};

struct channel {
	struct {
		uint16_t counter:10;
		uint16_t unused:6;
	};
	bool bit;
	bool output;
};

struct latch_data_cmd {
	uint8_t data:4;
	uint8_t type:1;
	uint8_t channel:2;
	uint8_t unused:1;
};

struct data_cmd {
	uint8_t data:6;
	uint8_t unused:2;
};

union command {
	uint8_t raw;
	union {
		struct {
			uint8_t contents:7;
			uint8_t cmd_type:1;
		};
		struct latch_data_cmd latch_data_cmd;
		struct data_cmd data_cmd;
	};
};

struct sn76489 {
	union volume_register vol_regs[NUM_CHANNELS];
	union tone_register tone_regs[NUM_TONE_CHANNELS];
	union noise_register noise_reg;
	struct channel channels[NUM_CHANNELS];
	uint16_t lfsr;
	uint8_t current_reg_type;
	uint8_t current_channel;
	struct port_region region;
	struct clock clock;
};

static bool sn76489_init(struct controller_instance *instance);
static void sn76489_reset(struct controller_instance *instance);
static void sn76489_deinit(struct controller_instance *instance);
static void sn76489_write(struct sn76489 *sn76489, uint8_t b);
static void handle_tone_channel(struct sn76489 *sn76489, int channel);
static void handle_noise_channel(struct sn76489 *sn76489);
static void mix(struct sn76489 *sn76489);
static void sn76489_tick(struct sn76489 *sn76489);

static struct pops sn76489_pops = {
	.write = (write_t)sn76489_write
};

void sn76489_write(struct sn76489 *sn76489, uint8_t b)
{
	union command cmd;
	uint8_t channel;

	/* Fill command parameters */
	cmd.raw = b;

	/* Handle LATCH/DATA command */
	if (cmd.cmd_type == LATCH_DATA_CMD_TYPE) {
		/* Extract channel */
		channel = cmd.latch_data_cmd.channel;

		/* The 4 data bits are placed into the low 4 bits of the
		relevant register. For the three-bit noise register, the highest
		bit is discarded. */
		if (cmd.latch_data_cmd.type == LATCH_VOLUME_CMD_TYPE)
			bitops_setb(&sn76489->vol_regs[channel].raw,
				0,
				4,
				cmd.latch_data_cmd.data);
		else if (channel != NOISE_CHANNEL)
			bitops_setw(&sn76489->tone_regs[channel].raw,
				0,
				4,
				cmd.latch_data_cmd.data);
		else
			bitops_setb(&sn76489->noise_reg.raw,
				0,
				3,
				cmd.latch_data_cmd.data);

		/* Save register type and channel */
		sn76489->current_reg_type = cmd.latch_data_cmd.type;
		sn76489->current_channel = channel;
	}

	/* Handle DATA command */
	if (cmd.cmd_type == DATA_CMD_TYPE) {
		/* Get latched channel */
		channel = sn76489->current_channel;

		/* If the currently latched register is a tone register then the
		low 6 bits of the byte are placed into the high 6 bits of the
		latched register. If the currently latched register is a tone
		register then the low 6 bits of the byte are placed into the
		high 6 bits of the latched register. */
		if (sn76489->current_reg_type == LATCH_VOLUME_CMD_TYPE)
			bitops_setb(&sn76489->vol_regs[channel].raw,
				0,
				4,
				cmd.data_cmd.data);
		else if (channel != NOISE_CHANNEL)
			bitops_setw(&sn76489->tone_regs[channel].raw,
				4,
				6,
				cmd.data_cmd.data);
		else
			bitops_setb(&sn76489->noise_reg.raw,
				0,
				4,
				cmd.data_cmd.data);
	}

	/* Reset shift register if needed */
	if ((sn76489->current_reg_type == LATCH_TONE_CMD_TYPE) &&
		(channel == NOISE_CHANNEL)) {
		sn76489->lfsr = 0;
		bitops_setw(&sn76489->lfsr, 15, 1, 1);
	}
}

void handle_tone_channel(struct sn76489 *sn76489, int channel)
{
	uint16_t counter;

	/* The counter is reset to the value currently in the corresponding
	register (eg. Tone0 for channel 0). */
	counter = sn76489->tone_regs[channel].reset_value;
	sn76489->channels[channel].counter = counter;

	/* If the register value is zero or one then the output is a constant
	value of +1. This is often used for sample playback. */
	if ((counter == 0) || (counter == 1)) {
		sn76489->channels[channel].bit = true;
		sn76489->channels[channel].output = true;
		return;
	}

	/* The output bit is flipped - if it is currently outputting 1, it
	changes to 0, and vice versa. */
	sn76489->channels[channel].bit = !sn76489->channels[channel].bit;
	sn76489->channels[channel].output = sn76489->channels[channel].bit;
}

void handle_noise_channel(struct sn76489 *sn76489)
{
	struct channel *channel = &sn76489->channels[NOISE_CHANNEL];
	bool bit;

	/* Reset counter according to the low 2 bits of the noise register */
	switch (sn76489->noise_reg.shift_rate) {
	case 0x00:
		channel->counter = 0x10;
		break;
	case 0x01:
		channel->counter = 0x20;
		break;
	case 0x02:
		channel->counter = 0x40;
		break;
	case 0x03:
		channel->counter = sn76489->tone_regs[2].reset_value;
		break;
	}

	/* As with the tone channels, the output bit is toggled between 0 and 1.
	However, this is not sent to the mixer, but to a "linear feedback shift
	register" (LFSR), which can generate noise or act as a divider. */
	channel->bit = !channel->bit;

	/* Handle LFSR only when input changes from 0 to 1 */
	if (!channel->bit)
		return;

	/* Handle white or periodic noise depending on mode */
	if (sn76489->noise_reg.mode == WHITE_NOISE) {
		/* The input bit is determined by an XOR feedback network.
		Certain bits are used as inputs to the XOR gates; these are the
		"tapped" bits. */
		bit = bitops_parity(sn76489->lfsr & TAP_MASK);
	} else {
		/* Bit 0 is tapped (the output bit is also the input bit) */
		bit = bitops_getw(&sn76489->lfsr, 0, 1);
	}

	/* Output bit 0 to the mixer */
	channel->output = bitops_getw(&sn76489->lfsr, 0, 1);

	/* Shift array by one bit and add input bit */
	sn76489->lfsr >>= 1;
	bitops_setw(&sn76489->lfsr, 15, 1, bit);
}

void mix(struct sn76489 *sn76489)
{
	uint8_t vol;
	uint8_t att;
	uint8_t final_volume;
	int channel;

	/* The mixer multiplies each channel's output by the corresponding
	volume (or, equivalently, applies the corresponding attenuation), and
	sums them. */
	final_volume = 0;
	for (channel = 0; channel < NUM_CHANNELS; channel++) {
		/* Skip channel if needed */
		if (!sn76489->channels[channel].output)
			continue;

		/* Set volume based on attenuation */
		att = sn76489->vol_regs[channel].attenuation;
		vol = MAX_VOLUME * (MAX_ATTENUATION - att) / MAX_ATTENUATION;

		/* Add channel output */
		final_volume += vol / NUM_CHANNELS;
	};

	/* Enqueue mixer output */
	audio_enqueue(&final_volume, 1);
}

void sn76489_tick(struct sn76489 *sn76489)
{
	int channel;

	/* Cycle through channels */
	for (channel = 0; channel < NUM_CHANNELS; channel++) {
		/* Decrement channel counter if non-zero */
		if (sn76489->channels[channel].counter > 0)
			sn76489->channels[channel].counter--;

		/* Skip channel if counter is still non-zero */
		if (sn76489->channels[channel].counter != 0)
			continue;

		/* Handle tone channel */
		if (channel != NOISE_CHANNEL)
			handle_tone_channel(sn76489, channel);
		else
			handle_noise_channel(sn76489);
	}

	/* Mix all channels */
	mix(sn76489);

	/* Always consume a single cycle */
	clock_consume(1);
}

bool sn76489_init(struct controller_instance *instance)
{
	struct sn76489 *sn76489;
	struct audio_specs audio_specs;
	struct resource *res;

	/* Allocate SN76489 structure */
	instance->priv_data = calloc(1, sizeof(struct sn76489));
	sn76489 = instance->priv_data;

	/* Set up port region */
	res = resource_get("port",
		RESOURCE_PORT,
		instance->resources,
		instance->num_resources);
	sn76489->region.area = res;
	sn76489->region.pops = &sn76489_pops;
	sn76489->region.data = sn76489;
	port_region_add(&sn76489->region);

	/* Add clock */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	sn76489->clock.rate = res->data.clk / INTERNAL_DIVIDER;
	sn76489->clock.data = sn76489;
	sn76489->clock.tick = (clock_tick_t)sn76489_tick;
	sn76489->clock.enabled = true;
	clock_add(&sn76489->clock);

	/* Initialize audio frontend */
	audio_specs.freq = sn76489->clock.rate;
	audio_specs.format = AUDIO_FORMAT_U8;
	audio_specs.channels = 1;
	if (!audio_init(&audio_specs)) {
		free(sn76489);
		return false;
	}

	return true;
}

void sn76489_reset(struct controller_instance *instance)
{
	struct sn76489 *sn76489 = instance->priv_data;
	int channel;

	/* Reset all attenuations and channel counters */
	for (channel = 0; channel < NUM_CHANNELS; channel++) {
		sn76489->vol_regs[channel].attenuation = MAX_ATTENUATION;
		sn76489->channels[channel].counter = 0;
	}
}

void sn76489_deinit(struct controller_instance *instance)
{
	audio_deinit();
	free(instance->priv_data);
}

CONTROLLER_START(sn76489)
	.init = sn76489_init,
	.reset = sn76489_reset,
	.deinit = sn76489_deinit
CONTROLLER_END

