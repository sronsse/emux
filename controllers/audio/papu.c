#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <audio.h>
#include <bitops.h>
#include <clock.h>
#include <controller.h>
#include <memory.h>

#define NUM_REGS		23
#define NR10			0x00
#define NR11			0x01
#define NR12			0x02
#define NR13			0x03
#define NR14			0x04
#define NR21			0x06
#define NR22			0x07
#define NR23			0x08
#define NR24			0x09
#define NR30			0x0A
#define NR31			0x0B
#define NR32			0x0C
#define NR33			0x0D
#define NR34			0x0E
#define NR41			0x10
#define NR42			0x11
#define NR43			0x12
#define NR44			0x13
#define NR50			0x14
#define NR51			0x15
#define NR52			0x16

#define NUM_CHANNELS		4
#define NUM_SQUARE_STEPS	8
#define NUM_FRAME_SEQ_STEPS	8
#define MAX_SQUARE_LEN_COUNTER	64
#define MAX_WAVE_LEN_COUNTER	256
#define MAX_NOISE_LEN_COUNTER	64
#define MAX_FREQ		2048
#define MAX_VOLUME		0x0F
#define FRAME_SEQ_RATE		512
#define SQUARE_FREQ_MUL		4
#define WAVE_RAM_SIZE		16
#define WAVE_FREQ_MUL		2

union channel_sweep {
	uint8_t raw;
	struct {
		uint8_t num_shift:3;
		uint8_t inc_dec:1;
		uint8_t time:3;
		uint8_t reserved:1;
	};
};

union channel_len {
	uint8_t raw;
	struct {
		uint8_t data:6;
		uint8_t wave_duty:2;
	};
};

union channel_env {
	uint8_t raw;
	struct {
		uint8_t num_sweep:3;
		uint8_t dir:1;
		uint8_t initial_volume:4;
	};
};

union channel_freq {
	uint8_t raw;
	struct {
		uint8_t freq_high:3;
		uint8_t reserved:3;
		uint8_t counter_sel:1;
		uint8_t initial:1;
	};
};

union channel_on_off {
	uint8_t raw;
	struct {
		uint8_t reserved:7;
		uint8_t on:1;
	};
};

union channel_lvl {
	uint8_t raw;
	struct {
		uint8_t reserved1:5;
		uint8_t level:2;
		uint8_t reserved2:1;
	};
};

union channel_poly {
	uint8_t raw;
	struct {
		uint8_t ratio:3;
		uint8_t width:1;
		uint8_t shift:4;
	};
};

union channel_ctrl {
	uint8_t raw;
	struct {
		uint8_t so1_lvl:3;
		uint8_t so1_vin:1;
		uint8_t so2_lvl:3;
		uint8_t so2_vin:1;
	};
};

union sound_sel {
	uint8_t raw;
	struct {
		uint8_t snd1_so1:1;
		uint8_t snd2_so1:1;
		uint8_t snd3_so1:1;
		uint8_t snd4_so1:1;
		uint8_t snd1_so2:1;
		uint8_t snd2_so2:1;
		uint8_t snd3_so2:1;
		uint8_t snd4_so2:1;
	};
};

union sound_ctrl {
	uint8_t raw;
	struct {
		uint8_t snd1_on:1;
		uint8_t snd2_on:1;
		uint8_t snd3_on:1;
		uint8_t snd4_on:1;
		uint8_t reserved:3;
		uint8_t all_on:1;
	};
};

union papu_regs {
	uint8_t raw[NUM_REGS];
	struct {
		union channel_sweep nr10;
		union channel_len nr11;
		union channel_env nr12;
		uint8_t nr13;
		union channel_freq nr14;
		uint8_t reserved1;
		union channel_len nr21;
		union channel_env nr22;
		uint8_t nr23;
		union channel_freq nr24;
		union channel_on_off nr30;
		uint8_t nr31;
		union channel_lvl nr32;
		uint8_t nr33;
		union channel_freq nr34;
		uint8_t reserved2;
		union channel_len nr41;
		union channel_env nr42;
		union channel_poly nr43;
		union channel_freq nr44;
		union channel_ctrl nr50;
		union sound_sel nr51;
		union sound_ctrl nr52;
	};
};

struct channel1 {
	uint8_t value;
	uint8_t step;
	uint8_t volume;
	uint16_t counter;
	uint8_t len_counter;
	uint8_t env_counter;
	uint16_t sweep_freq;
	uint16_t sweep_counter;
	uint16_t lfsr;
	bool enabled;
	bool sweep_enabled;
};

struct channel2 {
	bool enabled;
	uint8_t value;
	uint8_t step;
	uint8_t volume;
	uint16_t counter;
	uint8_t len_counter;
	uint8_t env_counter;
};

struct channel3 {
	bool enabled;
	uint8_t pos;
	uint8_t sample;
	uint16_t counter;
	uint16_t len_counter;
};

struct channel4 {
	bool enabled;
	uint8_t value;
	uint8_t volume;
	uint16_t counter;
	uint8_t len_counter;
	uint8_t env_counter;
	uint16_t lfsr;
};

struct papu {
	union papu_regs regs;
	struct channel1 channel1;
	struct channel2 channel2;
	struct channel3 channel3;
	struct channel4 channel4;
	uint8_t wave_ram[WAVE_RAM_SIZE];
	struct region region;
	struct region wave_region;
	struct clock main_clock;
};

static bool papu_init(struct controller_instance *instance);
static void papu_reset(struct controller_instance *instance);
static void papu_deinit(struct controller_instance *instance);
static uint8_t papu_readb(struct papu *papu, address_t address);
static void papu_writeb(struct papu *papu, uint8_t b, address_t address);
static void channel1_write(struct papu *papu, address_t address);
static void channel2_write(struct papu *papu, address_t address);
static void channel3_write(struct papu *papu, address_t address);
static void channel4_write(struct papu *papu, address_t address);
static void papu_tick(struct papu *papu);
static void square_update(struct channel *channel, uint16_t freq, uint8_t duty);

static struct mops papu_mops = {
	.readb = (readb_t)papu_readb,
	.writeb = (writeb_t)papu_writeb
};

uint8_t papu_readb(struct papu *papu, address_t address)
{
	uint8_t b;

	/* Read requested register */
	b = papu->regs.raw[address];

	/* Handle each register read (OR'ing it with specific values) */
	switch (address) {
	case NR10:
		return b | 0x80;
	case NR11:
	case NR21:
		return b | 0x3F;
	case NR12:
	case NR22:
	case NR42:
	case NR43:
	case NR50:
	case NR51:
		return b;
	case NR14:
	case NR24:
	case NR34:
	case NR44:
		return b | 0xBF;
	case NR30:
		return b | 0x7F;
	case NR32:
		return b | 0x9F;
	case NR52:
		return b | 0x70;
	default:
		return 0xFF;
	}
}

void papu_writeb(struct papu *papu, uint8_t b, address_t address)
{
	union sound_ctrl sound_ctrl;

	/* Handle power control */
	if (address == NR52) {
		sound_ctrl.raw = b;

		/* Write 0 to all registers */
		if (papu->regs.nr52.all_on && !sound_ctrl.all_on)
			memset(papu->regs.raw, 0, NUM_REGS * sizeof(uint8_t));

		/* Update sound control register */
		papu->regs.nr52.all_on = sound_ctrl.all_on;
	}

	/* Leave already if power is off */
	if (!papu->regs.nr52.all_on)
		return;

	/* Write to requested register */
	papu->regs.raw[address] = b;

	/* Handle writes to each channel separately */
	switch (address) {
	case NR10:
	case NR11:
	case NR12:
	case NR13:
	case NR14:
		channel1_write(papu, address);
		break;
	case NR21:
	case NR22:
	case NR23:
	case NR24:
		channel2_write(papu, address);
		break;
	case NR30:
	case NR31:
	case NR32:
	case NR33:
	case NR34:
		channel3_write(papu, address);
		break;
	case NR41:
	case NR42:
	case NR43:
	case NR44:
		channel4_write(papu, address);
		break;
	default:
		break;
	}
}

void channel1_write(struct papu *papu, address_t address)
{
	uint8_t v;
	uint16_t freq;
	uint16_t counter;
	uint16_t d;
	bool dac_on;
	bool dec;

	switch (address) {
	case NR11:
		/* Reload length counter */
		v = MAX_SQUARE_LEN_COUNTER - papu->regs.nr11.data;
		papu->channel1.len_counter = v;
		break;
	case NR12:
		/* Handle DAC power off */
		dac_on = bitops_getb(&papu->regs.nr12.raw, 3, 5);
		if (!dac_on)
			papu->channel1.enabled = false;
		break;
	case NR14:
		/* Break if trigger event is not required */
		if (!papu->regs.nr14.initial)
			break;

		/* Enable channel if DAC is on (controlled by NR12) */
		dac_on = bitops_getb(&papu->regs.nr12.raw, 3, 5);
		papu->channel1.enabled = dac_on;

		/* Reload length counter if needed */
		if (papu->channel1.len_counter == 0)
			papu->channel1.len_counter = MAX_SQUARE_LEN_COUNTER;

		/* Reload frequency timer with period */
		freq = papu->regs.nr13 | (papu->regs.nr14.freq_high << 8);
		papu->channel1.counter = (MAX_FREQ - freq) * SQUARE_FREQ_MUL;

		/* Reload volume envelope timer with period */
		papu->channel1.env_counter = papu->regs.nr12.num_sweep;

		/* Reload channel volume */
		papu->channel1.volume = papu->regs.nr12.initial_volume;

		/* Copy frequency shadow register and reload sweep timer */
		papu->channel1.sweep_freq = freq;
		papu->channel1.sweep_counter = papu->regs.nr10.time;

		/* Set/reset internal enabled flag based on period and shift */
		papu->channel1.sweep_enabled = (papu->regs.nr10.time != 0) ||
			(papu->regs.nr10.num_shift != 0);

		/* Perform frequency calculation and overflow check if needed */
		if (papu->regs.nr10.time != 0) {
			/* Update sweep shadow frequency */
			v = papu->regs.nr10.num_shift;
			dec = papu->regs.nr10.inc_dec;
			d = papu->channel1.sweep_freq >> v;
			papu->channel1.sweep_freq += dec ? -d : d;

			/* Write frequency back to channel registers */
			freq = papu->channel1.sweep_freq;
			papu->regs.nr13 = freq & 0xFF;
			papu->regs.nr14.freq_high = freq >> 8;

			/* Reload frequency timer */
			counter = (MAX_FREQ - freq) * SQUARE_FREQ_MUL;
			papu->channel1.counter = counter;

			/* Disable channel if maximum frequency is reached */
			if (papu->channel1.sweep_freq >= MAX_FREQ) {
				papu->channel1.enabled = false;
				papu->channel1.sweep_enabled = false;
			}
		}
		break;
	default:
		break;
	}
}

void channel2_write(struct papu *papu, address_t address)
{
	uint8_t v;
	uint16_t freq;
	bool dac_on;

	switch (address) {
	case NR21:
		/* Reload length counter */
		v = MAX_SQUARE_LEN_COUNTER - papu->regs.nr21.data;
		papu->channel2.len_counter = v;
		break;
	case NR22:
		/* Handle DAC power off */
		dac_on = bitops_getb(&papu->regs.nr22.raw, 3, 5);
		if (!dac_on)
			papu->channel2.enabled = false;
		break;
	case NR24:
		/* Break if trigger event is not required */
		if (!papu->regs.nr24.initial)
			break;

		/* Enable channel if DAC is on (controlled by NR22) */
		dac_on = bitops_getb(&papu->regs.nr22.raw, 3, 5);
		papu->channel2.enabled = dac_on;

		/* Reload length counter if needed */
		if (papu->channel2.len_counter == 0)
			papu->channel2.len_counter = MAX_SQUARE_LEN_COUNTER;

		/* Reload frequency timer with period */
		freq = papu->regs.nr23 | (papu->regs.nr24.freq_high << 8);
		papu->channel2.counter = (MAX_FREQ - freq) * SQUARE_FREQ_MUL;

		/* Reload volume envelope timer with period */
		papu->channel2.env_counter = papu->regs.nr22.num_sweep;

		/* Reload channel volume */
		papu->channel2.volume = papu->regs.nr22.initial_volume;
		break;
	default:
		break;
	}
}

void channel3_write(struct papu *papu, address_t address)
{
	uint8_t v;
	uint16_t freq;

	switch (address) {
	case NR30:
		/* Handle DAC power off */
		if (!papu->regs.nr30.on)
			papu->channel3.enabled = false;
		break;
	case NR31:
		/* Reload length counter */
		v = MAX_WAVE_LEN_COUNTER - papu->regs.nr31;
		papu->channel3.len_counter = v;
		break;
	case NR34:
		/* Break if trigger event is not required */
		if (!papu->regs.nr34.initial)
			break;

		/* Enable channel if DAC is on (controlled by NR30) */
		papu->channel3.enabled = papu->regs.nr30.on;

		/* Reload length counter if needed */
		if (papu->channel3.len_counter == 0)
			papu->channel3.len_counter = MAX_WAVE_LEN_COUNTER;

		/* Reload frequency timer with period */
		freq = papu->regs.nr33 | (papu->regs.nr34.freq_high << 8);
		papu->channel3.counter = (MAX_FREQ - freq) * WAVE_FREQ_MUL;
		break;
	default:
		break;
	}
}

void channel4_write(struct papu *papu, address_t address)
{
	uint8_t v;
	uint16_t counter;
	bool dac_on;

	switch (address) {
	case NR41:
		/* Reload length counter */
		v = MAX_NOISE_LEN_COUNTER - papu->regs.nr41.data;
		papu->channel4.len_counter = v;
		break;
	case NR42:
		/* Handle DAC power off */
		dac_on = bitops_getb(&papu->regs.nr42.raw, 3, 5);
		if (!dac_on)
			papu->channel4.enabled = false;
		break;
	case NR44:
		/* Break if trigger event is not required */
		if (!papu->regs.nr44.initial)
			break;

		/* Enable channel if DAC is on (controlled by NR42) */
		dac_on = bitops_getb(&papu->regs.nr42.raw, 3, 5);
		papu->channel4.enabled = dac_on;

		/* Reload length counter if needed */
		if (papu->channel4.len_counter == 0)
			papu->channel4.len_counter = MAX_NOISE_LEN_COUNTER;

		/* Reload frequency timer with period */
		counter = noise_get_divisor(papu) << papu->regs.nr43.shift;
		papu->channel4.counter = counter;

		/* Reload volume envelope timer with period */
		papu->channel4.env_counter = papu->regs.nr42.num_sweep;

		/* Reload channel volume */
		papu->channel4.volume = papu->regs.nr42.initial_volume;

		/* Reset LFSR */
		papu->channel4.lfsr = 0x7FFF;
		break;
	default:
		break;
	}
}

void square_update(struct channel *channel, uint16_t freq, uint8_t duty)
{
	uint8_t s;

	/* Leave already if channel is disabled */
	if (!channel->enabled)
		return;

	/* Check if channel needs update */
	if (channel->counter == 0) {
		/* Reset counter based on frequency */
		channel->counter = (2048 - freq) * 4;

		/* Update channel value based on following duty cycles:
		Duty   Waveform    Ratio
		0      00000001    12.5%
		1      10000001    25%
		2      10000111    50%
		3      01111110    75% */
		s = channel->step;
		switch (duty) {
		case 0:
			channel->value = (s == 7);
			break;
		case 1:
			channel->value = ((s == 0) || (s == 7));
			break;
		case 2:
			channel->value = ((s == 0) || (s >= 5));
			break;
		case 3:
			channel->value = ((s >= 1) && (s <= 6));
			break;
		}

		/* Increment step and handle overflow */
		if (++channel->step == 8)
			channel->step = 0;
	}

	/* Decrement channel counter */
	channel->counter--;
}

void papu_tick(struct papu *papu)
{
	uint8_t buffer[2];
	uint8_t value;
	uint16_t freq;
	uint8_t duty;

	/* Update square channel 1 */
	freq = papu->regs.nr13 | (papu->regs.nr14.freq_high << 8);
	duty = papu->regs.nr11.wave_duty;
	square_update(&papu->channel1, freq, duty);

	/* Update square channel 2 */
	freq = papu->regs.nr23 | (papu->regs.nr24.freq_high << 8);
	duty = papu->regs.nr21.wave_duty;
	square_update(&papu->channel2, freq, duty);

	/* Compute final value */
	/* TODO: split left and right */
	value = (papu->channel1.value + papu->channel2.value) * UCHAR_MAX / 2;
	buffer[0] = value;
	buffer[1] = value;
	audio_enqueue(buffer, 1);

	/* Always consume one cycle */
	clock_consume(1);
}

bool papu_init(struct controller_instance *instance)
{
	struct papu *papu;
	struct audio_specs audio_specs;
	struct resource *res;

	/* Allocate PAPU structure */
	instance->priv_data = malloc(sizeof(struct papu));
	papu = instance->priv_data;

	/* Add PAPU memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	papu->region.area = res;
	papu->region.mops = &papu_mops;
	papu->region.data = papu;
	memory_region_add(&papu->region);

	/* Add wave RAM region */
	res = resource_get("wave",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	papu->wave_region.area = res;
	papu->wave_region.mops = &ram_mops;
	papu->wave_region.data = papu->wave_ram;
	memory_region_add(&papu->wave_region);

	/* Add main clock */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	papu->main_clock.rate = res->data.clk;
	papu->main_clock.data = papu;
	papu->main_clock.tick = (clock_tick_t)papu_tick;
	papu->main_clock.enabled = true;
	clock_add(&papu->main_clock);

	/* Initialize audio frontend */
	audio_specs.freq = papu->main_clock.rate;
	audio_specs.format = AUDIO_FORMAT_U8;
	audio_specs.channels = 2;
	if (!audio_init(&audio_specs)) {
		free(papu);
		return false;
	}

	return true;
}

void papu_reset(struct controller_instance *instance)
{
	struct papu *papu = instance->priv_data;

	/* Initialize controller data */
	memset(papu->regs.raw, 0, NUM_REGS * sizeof(uint8_t));
	memset(&papu->channel1, 0, sizeof(struct channel1));
	memset(&papu->channel2, 0, sizeof(struct channel2));
	memset(&papu->channel3, 0, sizeof(struct channel3));
	memset(&papu->channel4, 0, sizeof(struct channel4));
	papu->seq_step = 0;

	/* Initialize noise channel linear feedback shift register */
	papu->channel4.lfsr = 0x7FFF;
}

void papu_deinit(struct controller_instance *instance)
{
	audio_deinit();
	free(instance->priv_data);
}

CONTROLLER_START(papu)
	.init = papu_init,
	.reset = papu_reset,
	.deinit = papu_deinit
CONTROLLER_END

