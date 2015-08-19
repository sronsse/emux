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
	uint8_t seq_step;
	uint8_t wave_ram[WAVE_RAM_SIZE];
	struct region region;
	struct region wave_region;
	struct clock main_clock;
	struct clock seq_clock;
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
static void seq_tick(struct papu *papu);
static void length_counter_tick(struct papu *papu);
static void vol_env_tick(struct papu *papu);
static void square1_update(struct papu *papu);
static void square2_update(struct papu *papu);
static void wave_update(struct papu *papu);
static void noise_update(struct papu *papu);
static uint8_t noise_get_divisor(struct papu *papu);

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

void square1_update(struct papu *papu)
{
	uint16_t freq;
	uint8_t s;

	/* Leave already if channel is disabled (zeroing the output) */
	if (!papu->channel1.enabled) {
		papu->channel1.value = 0;
		return;
	}

	/* Check if channel needs update */
	if (papu->channel1.counter == 0) {
		/* Reset counter based on frequency */
		freq = papu->regs.nr13 | (papu->regs.nr14.freq_high << 8);
		papu->channel1.counter = (MAX_FREQ - freq) * SQUARE_FREQ_MUL;

		/* Update channel value based on following duty cycles:
		Duty   Waveform    Ratio
		------------------------
		0      00000001    12.5%
		1      10000001    25%
		2      10000111    50%
		3      01111110    75% */
		s = papu->channel1.step;
		switch (papu->regs.nr11.wave_duty) {
		case 0:
			papu->channel1.value = (s == 7);
			break;
		case 1:
			papu->channel1.value = ((s == 0) || (s == 7));
			break;
		case 2:
			papu->channel1.value = ((s == 0) || (s >= 5));
			break;
		case 3:
			papu->channel1.value = ((s >= 1) && (s <= 6));
			break;
		}

		/* Increment step and handle overflow */
		if (++papu->channel1.step == NUM_SQUARE_STEPS)
			papu->channel1.step = 0;
	}

	/* Decrement channel counter */
	papu->channel1.counter--;
}

void square2_update(struct papu *papu)
{
	uint16_t freq;
	uint8_t s;

	/* Leave already if channel is disabled (zeroing the output) */
	if (!papu->channel2.enabled) {
		papu->channel2.value = 0;
		return;
	}

	/* Check if channel needs update */
	if (papu->channel2.counter == 0) {
		/* Reset counter based on frequency */
		freq = papu->regs.nr23 | (papu->regs.nr24.freq_high << 8);
		papu->channel2.counter = (MAX_FREQ - freq) * SQUARE_FREQ_MUL;

		/* Update channel value based on following duty cycles:
		Duty   Waveform    Ratio
		------------------------
		0      00000001    12.5%
		1      10000001    25%
		2      10000111    50%
		3      01111110    75% */
		s = papu->channel2.step;
		switch (papu->regs.nr21.wave_duty) {
		case 0:
			papu->channel2.value = (s == 7);
			break;
		case 1:
			papu->channel2.value = ((s == 0) || (s == 7));
			break;
		case 2:
			papu->channel2.value = ((s == 0) || (s >= 5));
			break;
		case 3:
			papu->channel2.value = ((s >= 1) && (s <= 6));
			break;
		}

		/* Increment step and handle overflow */
		if (++papu->channel2.step == NUM_SQUARE_STEPS)
			papu->channel2.step = 0;
	}

	/* Decrement channel counter */
	papu->channel2.counter--;
}

void wave_update(struct papu *papu)
{
	uint16_t freq;
	uint8_t sample;
	uint8_t addr;
	uint8_t shift;

	/* Leave already if channel is disabled (zeroing the sample) */
	if (!papu->channel3.enabled) {
		papu->channel3.sample = 0;
		return;
	}

	/* Check if channel needs update */
	if (papu->channel3.counter == 0) {
		/* Reset counter based on frequency */
		freq = papu->regs.nr33 | (papu->regs.nr34.freq_high << 8);
		papu->channel3.counter = (MAX_FREQ - freq) * WAVE_FREQ_MUL;

		/* When the timer generates a clock, the position counter is
		advanced one sample (half a byte) in the wave table, looping
		back to the beginning when it goes past the end. */
		if (++papu->channel3.pos == WAVE_RAM_SIZE * 2)
			papu->channel3.pos = 0;

		/* Read sample from this new position (each byte encodes two
		samples, the first in the high bits) */
		addr = papu->channel3.pos / 2;
		shift = (papu->channel3.pos % 2 != 0) ? 0 : 4;
		sample = bitops_getb(&papu->wave_ram[addr], shift, 4);

		/* The DAC receives the current value from the upper/lower
		nibble of the sample buffer, shifted right by the volume
		control.
		Code   Shift   Volume
		-----------------------
		0      4         0% (silent)
		1      0       100%
		2      1        50%
		3      2        25% */
		switch (papu->regs.nr32.level) {
		case 0:
			papu->channel3.sample = 0;
			break;
		case 1:
			papu->channel3.sample = sample;
			break;
		case 2:
			papu->channel3.sample = sample >> 1;
			break;
		case 3:
			papu->channel3.sample = sample >> 2;
			break;
		}
	}

	/* Decrement channel counter */
	papu->channel3.counter--;
}

uint8_t noise_get_divisor(struct papu *papu)
{
	/* The noise channel's frequency timer period is set by a base
	divisor shifted left some number of bits:
	Divisor code   Divisor
	----------------------
	0                8
	1               16
	2               32
	3               48
	4               64
	5               80
	6               96
	7              112 */
	switch (papu->regs.nr43.ratio) {
	case 0:
		return 8;
	case 1:
		return 16;
	case 2:
		return 32;
	case 3:
		return 48;
	case 4:
		return 64;
	case 5:
		return 80;
	case 6:
		return 96;
	case 7:
		return 112;
	}
}

void noise_update(struct papu *papu)
{
	uint8_t divisor;
	uint8_t b;

	/* Leave already if channel is disabled (zeroing the output) */
	if (!papu->channel4.enabled) {
		papu->channel4.value = 0;
		return;
	}

	/* Check if channel needs update */
	if (papu->channel4.counter == 0) {
		/* Update counter */
		divisor = noise_get_divisor(papu);
		papu->channel4.counter = divisor << papu->regs.nr43.shift;

		/* The linear feedback shift register (LFSR) generates a
		pseudo-random bit sequence. It has a 15-bit shift register with
		feedback. When clocked by the frequency timer, the low two bits
		(0 and 1) are XORed, all bits are shifted right by one, and the
		result of the XOR is put into the now-empty high bit. */
		b = bitops_getw(&papu->channel4.lfsr, 0, 1);
		b ^= bitops_getw(&papu->channel4.lfsr, 1, 1);
		papu->channel4.lfsr >>= 1;
		bitops_setw(&papu->channel4.lfsr, 14, 1, b);

		/* If width mode is 1 (NR43), the XOR result is also put into
		bit 6 after the shift, resulting in a 7-bit LFSR. */
		if (papu->regs.nr43.width)
			bitops_setw(&papu->channel4.lfsr, 6, 1, b);

		/* The waveform output is bit 0 of the LFSR, inverted. */
		papu->channel4.value = !bitops_getw(&papu->channel4.lfsr, 0, 1);
	}

	/* Decrement channel counter */
	papu->channel4.counter--;
}

void papu_tick(struct papu *papu)
{
	float ch1_output;
	float ch2_output;
	float ch3_output;
	float ch4_output;
	float left;
	float right;
	uint8_t buffer[2];

	/* Update square channels, wave channel, and noise channel */
	square1_update(papu);
	square2_update(papu);
	wave_update(papu);
	noise_update(papu);

	/* Compute square channel 1 output */
	ch1_output = papu->channel1.value;
	ch1_output *= (float)papu->channel1.volume / MAX_VOLUME;

	/* Compute square channel 2 output */
	ch2_output = papu->channel2.value;
	ch2_output *= (float)papu->channel2.volume / MAX_VOLUME;

	/* Compute wave channel output (sample has frequency and volume info) */
	ch3_output = (float)papu->channel3.sample / MAX_VOLUME;

	/* Compute noise channel output */
	ch4_output = papu->channel4.value;
	ch4_output *= (float)papu->channel4.volume / MAX_VOLUME;

	/* Mix all channels and compute left channel output */
	left = ch1_output * papu->regs.nr51.snd1_so2;
	left += ch2_output * papu->regs.nr51.snd2_so2;
	left += ch3_output * papu->regs.nr51.snd3_so2;
	left += ch4_output * papu->regs.nr51.snd4_so2;
	left /= NUM_CHANNELS;

	/* Mix all channels and compute right channel output */
	right = ch1_output * papu->regs.nr51.snd1_so1;
	right += ch2_output * papu->regs.nr51.snd2_so1;
	right += ch3_output * papu->regs.nr51.snd3_so1;
	right += ch4_output * papu->regs.nr51.snd4_so1;
	right /= NUM_CHANNELS;

	/* Enqueue audio data */
	buffer[0] = left * UCHAR_MAX;
	buffer[1] = right * UCHAR_MAX;
	audio_enqueue(buffer, 1);

	/* Always consume one cycle */
	clock_consume(1);
}

void length_counter_tick(struct papu *papu)
{
	bool handle_channel;

	/* Check if channel 1 length counter needs to be handled */
	handle_channel = papu->channel1.enabled &&
		papu->regs.nr14.counter_sel &&
		(papu->channel1.len_counter > 0);

	/* Decrement channel 1 counter and disable channel if needed */
	if (handle_channel)
		if (--papu->channel1.len_counter == 0)
			papu->channel1.enabled = false;

	/* Check if channel 2 length counter needs to be handled */
	handle_channel = papu->channel2.enabled &&
		papu->regs.nr24.counter_sel &&
		(papu->channel2.len_counter > 0);

	/* Decrement channel 2 counter and disable channel if needed */
	if (handle_channel)
		if (--papu->channel2.len_counter == 0)
			papu->channel2.enabled = false;

	/* Check if channel 3 length counter needs to be handled */
	handle_channel = papu->channel3.enabled &&
		papu->regs.nr34.counter_sel &&
		(papu->channel3.len_counter > 0);

	/* Decrement channel 3 counter and disable channel if needed */
	if (handle_channel)
		if (--papu->channel3.len_counter == 0)
			papu->channel3.enabled = false;

	/* Check if channel 4 length counter needs to be handled */
	handle_channel = papu->channel4.enabled &&
		papu->regs.nr44.counter_sel &&
		(papu->channel4.len_counter > 0);

	/* Decrement channel 4 counter and disable channel if needed */
	if (handle_channel)
		if (--papu->channel4.len_counter == 0)
			papu->channel4.enabled = false;
}

void vol_env_tick(struct papu *papu)
{
	/* Handle channel 1 volume envelope */
	if (papu->channel1.env_counter > 0) {
		/* Decrement counter and handle envelope if it reached 0 */
		if (--papu->channel1.env_counter == 0) {
			/* Increment/decrement volume based on direction */
			if (papu->regs.nr12.dir &&
				(papu->channel1.volume < MAX_VOLUME))
				papu->channel1.volume++;
			else if (!papu->regs.nr12.dir &&
				(papu->channel1.volume > 0))
				papu->channel1.volume--;

			/* Reload envelope counter */
			papu->channel1.env_counter = papu->regs.nr12.num_sweep;
		}
	}

	/* Handle channel 2 volume envelope */
	if (papu->channel2.env_counter > 0) {
		/* Decrement counter and handle envelope if it reached 0 */
		if (--papu->channel2.env_counter == 0) {
			/* Increment/decrement volume based on direction */
			if (papu->regs.nr22.dir &&
				(papu->channel2.volume < MAX_VOLUME))
				papu->channel2.volume++;
			else if (!papu->regs.nr22.dir &&
				(papu->channel2.volume > 0))
				papu->channel2.volume--;

			/* Reload envelope counter */
			papu->channel2.env_counter = papu->regs.nr22.num_sweep;
		}
	}

	/* Handle channel 4 volume envelope */
	if (papu->channel4.env_counter > 0) {
		/* Decrement counter and handle envelope if it reached 0 */
		if (--papu->channel4.env_counter == 0) {
			/* Increment/decrement volume based on direction */
			if (papu->regs.nr42.dir &&
				(papu->channel4.volume < MAX_VOLUME))
				papu->channel4.volume++;
			else if (!papu->regs.nr42.dir &&
				(papu->channel4.volume > 0))
				papu->channel4.volume--;

			/* Reload envelope counter */
			papu->channel4.env_counter = papu->regs.nr42.num_sweep;
		}
	}
}

void seq_tick(struct papu *papu)
{
	/* The frame sequencer generates low frequency clocks for the modulation
	units. It is clocked by a 512 Hz timer.
	Step  Length Ctr  Vol Env  Sweep
	--------------------------------
	0     Clock       -        -
	1     -           -        -
	2     Clock       -        Clock
	3     -           -        -
	4     Clock       -        -
	5     -           -        -
	6     Clock       -        Clock
	7     -           Clock    -
	--------------------------------
	Rate  256 Hz      64 Hz    128 Hz */
	switch (papu->seq_step) {
	case 0:
	case 4:
		length_counter_tick(papu);
		break;
	case 2:
	case 6:
		length_counter_tick(papu);
		sweep_tick(papu);
		break;
	case 7:
		vol_env_tick(papu);
		break;
	default:
		break;
	}

	/* Increment frame sequencer step and handle overflow */
	if (++papu->seq_step == NUM_FRAME_SEQ_STEPS)
		papu->seq_step = 0;

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

	/* Add frame sequencer clock */
	papu->seq_clock.rate = FRAME_SEQ_RATE;
	papu->seq_clock.data = papu;
	papu->seq_clock.tick = (clock_tick_t)seq_tick;
	papu->seq_clock.enabled = true;
	clock_add(&papu->seq_clock);

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

