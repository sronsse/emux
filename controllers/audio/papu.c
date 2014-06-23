#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <audio.h>
#include <bitops.h>
#include <clock.h>
#include <controller.h>
#include <log.h>
#include <memory.h>
#include <util.h>

#define NUM_REGS	23
#define NR10		0x00
#define NR11		0x01
#define NR12		0x02
#define NR13		0x03
#define NR14		0x04
#define NR21		0x06
#define NR22		0x07
#define NR23		0x08
#define NR24		0x09
#define NR30		0x0A
#define NR31		0x0B
#define NR32		0x0C
#define NR33		0x0D
#define NR34		0x0E
#define NR41		0x10
#define NR42		0x11
#define NR43		0x12
#define NR44		0x13
#define NR50		0x14
#define NR51		0x15
#define NR52		0x16

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
		uint8_t step:1;
		uint8_t freq:4;
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

struct channel {
	uint8_t value;
	uint8_t step;
	uint8_t volume;
	uint16_t counter;
	uint16_t len_counter;
	bool enabled;
};

struct papu {
	union papu_regs regs;
	struct channel channel1;
	struct channel channel2;
	struct region region;
	struct clock main_clock;
};

static bool papu_init(struct controller_instance *instance);
static void papu_reset(struct controller_instance *instance);
static void papu_deinit(struct controller_instance *instance);
static uint8_t papu_readb(struct papu *papu, address_t address);
static void papu_writeb(struct papu *papu, uint8_t b, address_t address);
static void channel1_write(struct papu *papu, uint8_t b, address_t address);
static void channel2_write(struct papu *papu, uint8_t b, address_t address);
static void papu_tick(struct papu *papu);
static void square_update(struct channel *channel, uint16_t freq, uint8_t duty);

static struct mops papu_mops = {
	.readb = (readb_t)papu_readb,
	.writeb = (writeb_t)papu_writeb
};

uint8_t papu_readb(struct papu *papu, address_t address)
{
	uint8_t b;
	union channel_sweep chan_sweep;
	union channel_len chan_len;
	union channel_freq chan_freq;
	union sound_ctrl sound_ctrl;
	union channel_on_off chan_on_off;
	union channel_lvl chan_lvl;

	/* Read register and process it (some fields are read back as 1) */
	switch (address) {
	case NR10:
		chan_sweep.raw = papu->regs.raw[address];
		chan_sweep.reserved = 1;
		b = chan_sweep.raw;
		break;
	case NR11:
	case NR21:
		chan_len.raw = papu->regs.raw[address];
		chan_len.data = 0x3F;
		b = chan_len.raw;
		break;
	case NR13:
	case NR23:
	case NR31:
	case NR33:
	case NR41:
		b = 0xFF;
		break;
	case NR14:
	case NR24:
	case NR34:
	case NR44:
		chan_freq.raw = papu->regs.raw[address];
		chan_freq.freq_high = 0x07;
		chan_freq.reserved = 0x07;
		chan_freq.initial = 1;
		b = chan_freq.raw;
		break;
	case NR30:
		chan_on_off.raw = papu->regs.raw[address];
		b = chan_on_off.raw;
		chan_on_off.reserved = 0x7F;
		b = chan_on_off.raw;
		break;
	case NR32:
		chan_lvl.raw = papu->regs.raw[address];
		chan_lvl.reserved1 = 0x1F;
		chan_lvl.reserved2 = 1;
		b = chan_lvl.raw;
		break;
	case NR52:
		sound_ctrl.raw = papu->regs.raw[address];
		sound_ctrl.reserved = 0x07;
		b = sound_ctrl.raw;
		break;
	default:
		b = papu->regs.raw[address];
		break;
	}

	/* Return processed register value */
	return b;
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
	/* TODO: move this */
	papu->regs.raw[address] = b;

	/* Handle writes to each channel separately */
	switch (address) {
	case NR10:
	case NR11:
	case NR12:
	case NR13:
	case NR14:
		channel1_write(papu, b, address);
		break;
	case NR21:
	case NR22:
	case NR23:
	case NR24:
		channel2_write(papu, b, address);
		break;
	default:
		break;
	}
}

void channel1_write(struct papu *papu, uint8_t UNUSED(b), address_t address)
{
	bool dac_on;

	switch (address) {
	case NR11:
		/* Reload length counter */
		papu->channel1.len_counter = papu->regs.nr11.data;
		break;
	case NR12:
		/* Handle DAC power off */
		dac_on = bitops_getb(&papu->regs.nr12.raw, 3, 5);
		if (!dac_on)
			papu->channel1.enabled = false;
		break;
	case NR14:
		/* Handle trigger event */
		if (papu->regs.nr14.initial) {
			/* Enable channel if DAC is on (controlled by NR12) */
			dac_on = bitops_getb(&papu->regs.nr12.raw, 3, 5);
			papu->channel1.enabled = dac_on;

			/* Reload length counter if needed */
			if (papu->channel1.len_counter == 0)
				papu->channel1.len_counter = 64;

			/* TODO:
			Frequency timer is reloaded with period.
			Volume envelope timer is reloaded with period.
			Channel volume is reloaded from NRx2.
			Square 1's sweep does several things. */
		}
		break;
	default:
		break;
	}
}

void channel2_write(struct papu *papu, uint8_t UNUSED(b), address_t address)
{
	bool dac_on;

	switch (address) {
	case NR21:
		/* Reload length counter */
		papu->channel1.len_counter = papu->regs.nr11.data;
		break;
	case NR22:
		/* Handle DAC power off */
		dac_on = bitops_getb(&papu->regs.nr22.raw, 3, 5);
		if (!dac_on)
			papu->channel2.enabled = false;
		break;
	case NR24:
		/* Handle trigger event */
		if (papu->regs.nr24.initial) {
			/* Enable channel if DAC is on (controlled by NR22) */
			dac_on = bitops_getb(&papu->regs.nr22.raw, 3, 5);
			papu->channel2.enabled = dac_on;

			/* Reload length counter if needed */
			if (papu->channel2.len_counter == 0)
				papu->channel2.len_counter = 64;

			/* TODO:
			Frequency timer is reloaded with period.
			Volume envelope timer is reloaded with period.
			Channel volume is reloaded from NRx2. */
		}
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

	/* Initialize registers and channels */
	memset(papu->regs.raw, 0, NUM_REGS * sizeof(uint8_t));
	papu->channel1.enabled = false;
	papu->channel2.enabled = false;
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

