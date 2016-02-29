#include <limits.h>
#include <string.h>
#include <audio.h>
#include <bitops.h>
#include <clock.h>
#include <controller.h>
#include <cpu.h>
#include <memory.h>
#include <util.h>

#define NUM_REGS		20
#define PULSE1_MAIN		0x00
#define PULSE1_SWEEP		0x01
#define PULSE1_TIMER_LOW	0x02
#define PULSE1_TIMER_HIGH	0x03
#define PULSE2_MAIN		0x04
#define PULSE2_SWEEP		0x05
#define PULSE2_TIMER_LOW	0x06
#define PULSE2_TIMER_HIGH	0x07
#define TRIANGLE_LINEAR_COUNTER	0x08
#define TRIANGLE_TIMER_HIGH	0x0A
#define TRIANGLE_TIMER_LOW	0x0B
#define NOISE_MAIN		0x0C
#define NOISE_PERIOD		0x0E
#define NOISE_LEN_COUNTER	0x0F
#define DMC_MAIN		0x10
#define DMC_DIRECT_LOAD		0x11
#define DMC_SAMPLE_ADDR		0x12
#define DMC_SAMPLE_LEN		0x13

#define NUM_PULSE_STEPS		8
#define NUM_TRIANGLE_STEPS	32
#define DMC_SAMPLE_ADDR_START	0xC000

struct pulse_main {
	uint8_t vol_env:4;
	uint8_t constant_vol:1;
	uint8_t env_loop_len_counter_halt:1;
	uint8_t duty:2;
};

struct pulse_sweep {
	uint8_t shift:3;
	uint8_t negate:1;
	uint8_t period:3;
	uint8_t enabled:1;
};

struct pulse_timer_high {
	uint8_t timer_high:3;
	uint8_t len_counter_load:5;
};

struct triangle_linear_counter {
	uint8_t reload_val:7;
	uint8_t control_len_counter_halt:1;
};

struct triangle_timer_high {
	uint8_t timer_high:3;
	uint8_t len_counter_load:5;
};

struct noise_main {
	uint8_t vol_env:4;
	uint8_t constant_vol:1;
	uint8_t env_loop_len_counter_halt:1;
	uint8_t unused:2;
};

struct noise_period {
	uint8_t period:4;
	uint8_t unused:3;
	uint8_t mode:1;
};

struct noise_len_counter {
	uint8_t unused:3;
	uint8_t load:5;
};

struct dmc_main {
	uint8_t freq_id:4;
	uint8_t unused:2;
	uint8_t loop_sample:1;
	uint8_t irq_enable:1;
};

struct dmc_load {
	uint8_t value:7;
	uint8_t unused:1;
};

union apu_ctrl {
	uint8_t raw;
	struct {
		uint8_t pulse1_len_counter_en:1;
		uint8_t pulse2_len_counter_en:1;
		uint8_t triangle_len_counter_en:1;
		uint8_t noise_len_counter_en:1;
		uint8_t dmc_en:1;
		uint8_t unused:3;
	};
};

union apu_stat {
	uint8_t raw;
	struct {
		uint8_t pulse1_len_counter_stat:1;
		uint8_t pulse2_len_counter_stat:1;
		uint8_t triangle_len_counter_stat:1;
		uint8_t noise_len_counter_stat:1;
		uint8_t dmc_active:1;
		uint8_t unused:1;
		uint8_t frame_interrupt:1;
		uint8_t dmc_interrupt:1;
	};
};

union seq {
	uint8_t raw;
	struct {
		uint8_t unused:6;
		uint8_t int_inhibit:1;
		uint8_t mode:1;
	};
};

struct apu_regs {
	union {
		uint8_t raw[NUM_REGS];
		struct {
			struct pulse_main pulse1_main;
			struct pulse_sweep pulse1_sweep;
			uint8_t pulse1_t_lo;
			struct pulse_timer_high pulse1_t_hi;
			struct pulse_main pulse2_main;
			struct pulse_sweep pulse2_sweep;
			uint8_t pulse2_t_lo;
			struct pulse_timer_high pulse2_t_hi;
			struct triangle_linear_counter triangle_linear_counter;
			uint8_t unused1;
			uint8_t triangle_t_lo;
			struct triangle_timer_high triangle_t_hi;
			struct noise_main noise_main;
			uint8_t unused2;
			struct noise_period noise_period;
			struct noise_len_counter noise_len_counter;
			struct dmc_main dmc_main;
			struct dmc_load dmc_load;
			uint8_t dmc_sample_addr;
			uint8_t dmc_sample_len;
		};
	};
	union apu_ctrl ctrl;
	union apu_stat stat;
	union seq seq;
};

struct pulse {
	bool len_counter_silenced;
	bool sweep_silenced;
	uint8_t value;
	uint8_t step;
	uint8_t volume;
	uint16_t counter;
	uint8_t len_counter;
	bool env_start;
	uint8_t env_counter;
	uint8_t env_period;
	bool sweep_reload;
	uint8_t sweep_counter;
};

struct triangle {
	bool len_counter_silenced;
	bool linear_counter_silenced;
	uint8_t value;
	uint8_t step;
	uint8_t volume;
	uint16_t counter;
	uint8_t len_counter;
	bool linear_counter_reload;
	uint8_t linear_counter;
};

struct noise {
	bool len_counter_silenced;
	uint8_t value;
	uint8_t volume;
	uint16_t counter;
	uint16_t shift_reg;
	uint8_t len_counter;
	bool env_start;
	uint8_t env_counter;
	uint8_t env_period;
};

struct dmc {
	bool silenced;
	uint16_t current_addr;
	uint16_t byte_count;
	bool sample_buffer_full;
	uint8_t sample_buffer;
	uint8_t shift_reg;
	uint8_t bits_remaining;
	uint16_t counter;
};

struct apu {
	struct apu_regs r;
	struct pulse pulse1;
	struct pulse pulse2;
	struct triangle triangle;
	struct noise noise;
	struct dmc dmc;
	int seq_step;
	int cycle;
	int bus_id;
	struct region main_region;
	struct region ctrl_stat_region;
	struct region seq_region;
	struct clock main_clock;
	struct clock seq_clock;
	int irq;
};

static bool apu_init(struct controller_instance *instance);
static void apu_reset(struct controller_instance *instance);
static void apu_deinit(struct controller_instance *instance);
static void apu_writeb(struct apu *apu, uint8_t b, address_t address);
static uint8_t stat_readb(struct apu *apu, address_t address);
static void ctrl_writeb(struct apu *apu, uint8_t b, address_t address);
static void seq_writeb(struct apu *apu, uint8_t b, address_t address);
static void apu_tick(struct apu *apu);
static void seq_tick(struct apu *apu);
static void length_counter_tick(struct apu *apu);
static void vol_env_tick(struct apu *apu);
static void sweep_tick(struct apu *apu);
static void linear_counter_tick(struct apu *apu);
static void pulse_update(struct apu *apu);
static void triangle_update(struct apu *apu);
static void noise_update(struct apu *apu);
static void dmc_update(struct apu *apu);

static struct mops apu_mops = {
	.writeb = (writeb_t)apu_writeb
};

static struct mops ctrl_stat_mops = {
	.readb = (readb_t)stat_readb,
	.writeb = (writeb_t)ctrl_writeb
};

static struct mops seq_mops = {
	.writeb = (writeb_t)seq_writeb
};

static uint8_t len_counter_table[] = {
	0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
	0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
	0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
	0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E
};

static uint8_t triangle_value_table[] = {
	0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
	0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

static uint16_t noise_period_table[] = {
	0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0060, 0x0080, 0x00A0,
	0x00CA, 0x00FE, 0x017C, 0x01FC, 0x02FA, 0x03F8, 0x07F2, 0x0FE4
};

static uint16_t dmc_rate_table[] = {
	0x01AC, 0x017C, 0x0154, 0x0140, 0x011E, 0x00FE, 0x00E2, 0x00D6,
	0x00BE, 0x00A0, 0x008E, 0x0080, 0x006A, 0x0054, 0x0048, 0x0036
};

void apu_writeb(struct apu *apu, uint8_t b, address_t address)
{
	uint8_t id;

	/* Write requested register */
	apu->r.raw[address] = b;

	/* Handle write */
	switch (address) {
	case PULSE1_TIMER_HIGH:
		/* Load pulse 1 length counter if enabled */
		if (apu->r.ctrl.pulse1_len_counter_en) {
			id = apu->r.pulse1_t_hi.len_counter_load;
			apu->pulse1.len_counter = len_counter_table[id];
			apu->pulse1.len_counter_silenced = false;
		}

		/* Set pulse 1 envelope start flag */
		apu->pulse1.env_start = true;
		break;
	case PULSE1_SWEEP:
		/* Set pulse 1 sweep reload flag */
		apu->pulse1.sweep_reload = true;
		break;
	case PULSE2_TIMER_HIGH:
		/* Load pulse 2 length counter if enabled */
		if (apu->r.ctrl.pulse2_len_counter_en) {
			id = apu->r.pulse2_t_hi.len_counter_load;
			apu->pulse2.len_counter = len_counter_table[id];
			apu->pulse2.len_counter_silenced = false;
		}

		/* Set pulse 2 envelope start flag */
		apu->pulse2.env_start = true;
		break;
	case PULSE2_SWEEP:
		/* Set pulse 2 sweep reload flag */
		apu->pulse2.sweep_reload = true;
		break;
	case TRIANGLE_TIMER_HIGH:
		/* Load triangle length counter if enabled */
		if (apu->r.ctrl.triangle_len_counter_en) {
			id = apu->r.triangle_t_hi.len_counter_load;
			apu->triangle.len_counter = len_counter_table[id];
			apu->triangle.len_counter_silenced = false;
		}

		/* Set triangle linear counter reload flag */
		apu->triangle.linear_counter_reload = true;
		break;
	case NOISE_LEN_COUNTER:
		/* Load noise length counter if enabled */
		if (apu->r.ctrl.noise_len_counter_en) {
			id = apu->r.noise_len_counter.load;
			apu->noise.len_counter = len_counter_table[id];
			apu->noise.len_counter_silenced = false;
		}

		/* Set noise envelope start flag */
		apu->noise.env_start = true;
		break;
	default:
		break;
	}
}

uint8_t stat_readb(struct apu *apu, address_t UNUSED(address))
{
	uint8_t b;

	/* Get current status register */
	b = apu->r.stat.raw;

	/* Clear frame interrupt flag */
	apu->r.stat.frame_interrupt = 0;

	/* Read old status register */
	return b;
}

void ctrl_writeb(struct apu *apu, uint8_t b, address_t UNUSED(address))
{
	/* Write control register */
	apu->r.ctrl.raw = b;

	/* The length counters can be disabled by clearing the appropriate bit
	in the control register, which immediately sets the counter to 0 and
	keeps it there. */
	if (!apu->r.ctrl.pulse1_len_counter_en) {
		apu->pulse1.len_counter = 0;
		apu->pulse1.len_counter_silenced = true;
	}
	if (!apu->r.ctrl.pulse2_len_counter_en) {
		apu->pulse2.len_counter = 0;
		apu->pulse2.len_counter_silenced = true;
	}
	if (!apu->r.ctrl.triangle_len_counter_en) {
		apu->triangle.len_counter = 0;
		apu->triangle.len_counter_silenced = true;
	}
	if (!apu->r.ctrl.noise_len_counter_en) {
		apu->noise.len_counter = 0;
		apu->noise.len_counter_silenced = true;
	}

	/* When a sample is started, the current address is set to the sample
	address, and bytes remaining is set to the sample length (the DMC sample
	will be restarted only if its bytes remaining is 0. If there are bits
	remaining in the 1-byte sample buffer, these will finish playing before
	the next sample is fetched. */
	if (apu->r.ctrl.dmc_en && (apu->dmc.byte_count == 0)) {
		/* Set current address ($C000 + (A * 64)) */
		apu->dmc.current_addr = DMC_SAMPLE_ADDR_START;
		apu->dmc.current_addr += apu->r.dmc_sample_addr * 64;

		/* Set bytes remaining ((L * 16) + 1) */
		apu->dmc.byte_count = (apu->r.dmc_sample_len * 16) + 1;
	}

	/* If the DMC bit is clear, the DMC bytes remaining will be set to 0 and
	the DMC will silence when it empties. */
	if (!apu->r.ctrl.dmc_en)
		apu->dmc.byte_count = 0;

	/* Writing to this register clears the DMC interrupt flag. */
	apu->r.stat.dmc_interrupt = 0;
}

void seq_writeb(struct apu *apu, uint8_t b, address_t UNUSED(address))
{
	/* Write frame sequencer */
	apu->r.seq.raw = b;

	/* On a write the sequencer, the divider and sequencer are reset. */
	apu->seq_clock.num_remaining_cycles = 0;
	apu->seq_step = 0;

	/* Clear frame interrupt flag upon setting the interrupt inhibit flag */
	if (apu->r.seq.int_inhibit)
		apu->r.stat.frame_interrupt = 0;
}

void pulse_update(struct apu *apu)
{
	struct pulse *pulse;
	struct pulse_main *p_main;
	struct pulse_timer_high *t_hi;
	uint8_t *t_lo;
	uint16_t period;
	uint8_t s;
	int c;

	/* Handle pulse 1 & 2 channels */
	for (c = 1; c <= 2; c++) {
		/* Get appropriate data/registers */
		pulse = (c == 1) ? &apu->pulse1 : &apu->pulse2;
		p_main = (c == 1) ? &apu->r.pulse1_main : &apu->r.pulse2_main;
		t_lo = (c == 1) ? &apu->r.pulse1_t_lo : &apu->r.pulse2_t_lo;
		t_hi = (c == 1) ? &apu->r.pulse1_t_hi : &apu->r.pulse2_t_hi;

		/* Continue if channel is disabled (zeroing the output) */
		if (pulse->len_counter_silenced || pulse->sweep_silenced) {
			pulse->value = 0;
			continue;
		}

		/* Check if pulse channel needs update */
		if (pulse->counter == 0) {
			/* Reset counter based on timer period */
			period = *t_lo;
			period |= t_hi->timer_high << 8;
			pulse->counter = period;

			/* Update pulse channel value based on following duty
			cycles:
			Duty   Waveform    Ratio
			------------------------
			0      01000000    12.5%
			1      01100000    25%
			2      01111000    50%
			3      10011111    25% negated */
			s = pulse->step;
			switch (p_main->duty) {
			case 0:
				pulse->value = (s == 1);
				break;
			case 1:
				pulse->value = ((s == 1) || (s == 2));
				break;
			case 2:
				pulse->value = ((s >= 1) && (s <= 4));
				break;
			case 3:
				pulse->value = ((s == 0) || (s >= 3));
				break;
			}

			/* Increment step and handle overflow */
			if (++pulse->step == NUM_PULSE_STEPS)
				pulse->step = 0;
		}

		/* Decrement pulse channel counter */
		pulse->counter--;
	}
}

void triangle_update(struct apu *apu)
{
	uint16_t period;
	bool silenced;

	/* Return if channel is disabled (zeroing the output) */
	silenced = apu->triangle.len_counter_silenced;
	silenced |= apu->triangle.linear_counter_silenced;
	if (silenced) {
		apu->triangle.value = 0;
		return;
	}

	/* Check if triangle channel needs update */
	if (apu->triangle.counter == 0) {
		/* Reset counter based on timer period */
		period = apu->r.triangle_t_lo;
		period |= apu->r.triangle_t_hi.timer_high << 8;
		apu->triangle.counter = period;

		/* Update triangle channel value based on table */
		apu->triangle.value = triangle_value_table[apu->triangle.step];

		/* Increment step and handle overflow */
		if (++apu->triangle.step == NUM_TRIANGLE_STEPS)
			apu->triangle.step = 0;
	}

	/* Decrement triangle channel counter */
	apu->triangle.counter--;
}

void noise_update(struct apu *apu)
{
	struct noise *noise = &apu->noise;
	bool mode;
	uint16_t feedback;

	/* Return if channel is disabled (zeroing the output) */
	if (noise->len_counter_silenced) {
		noise->value = 0;
		return;
	}

	/* Check if noise channel needs update */
	if (noise->counter == 0) {
		/* Reset counter based on timer period */
		noise->counter = noise_period_table[apu->r.noise_period.period];

		/* Feedback is calculated as the XOR of bit 0 and one other bit:
		bit 6 if mode flag is set, otherwise bit 1. */
		mode = apu->r.noise_period.mode;
		feedback = bitops_getw(&noise->shift_reg, 0, 1);
		feedback ^= bitops_getw(&noise->shift_reg, mode ? 6 : 1, 1);

		/* Shift shift register right by one bit */
		noise->shift_reg >>= 1;

		/* Set leftmost bit to the feedback calculated earlier */
		bitops_setw(&noise->shift_reg, 14, 1, feedback);

		/* Update channel value based on bit 0 of the shift register */
		noise->value = bitops_getw(&noise->shift_reg, 0, 1);
	}

	/* Decrement noise channel counter */
	noise->counter--;
}

void dmc_update(struct apu *apu)
{
	uint16_t addr;
	uint16_t len;
	uint8_t sample;
	int16_t target;
	bool inc;

	/* Check if memory reader needs to fill sample */
	if (!apu->dmc.sample_buffer_full && (apu->dmc.byte_count != 0)) {
		/* The sample buffer is filled with the next sample byte read
		from the current address, subject to whatever mapping hardware
		is present. */
		sample = memory_readb(apu->bus_id, apu->dmc.current_addr);
		apu->dmc.sample_buffer = sample;
		apu->dmc.sample_buffer_full = true;

		/* The address is incremented; if it exceeds $FFFF, it is
		wrapped around to $8000. */
		addr = apu->dmc.current_addr;
		addr = (addr != 0xFFFF) ? addr + 1 : 0x8000;
		apu->dmc.current_addr = addr;

		/* The bytes remaining counter is decremented; if it becomes
		zero and the loop flag is set, the sample is restarted;
		otherwise, if the bytes remaining counter becomes zero and the
		IRQ enabled flag is set, the interrupt flag is set. */
		apu->dmc.byte_count--;
		if (apu->dmc.byte_count == 0) {
			/* Check if loop flag is set */
			if (apu->r.dmc_main.loop_sample) {
				/* Set current address ($C000 + (A * 64)) */
				addr = DMC_SAMPLE_ADDR_START;
				addr += apu->r.dmc_sample_addr * 64;
				apu->dmc.current_addr = addr;

				/* Set bytes remaining */
				len = (apu->r.dmc_sample_len * 16) + 1;
				apu->dmc.byte_count = len;
			}

			/* Set interrupt flag if needed */
			if (apu->r.dmc_main.irq_enable)
				apu->r.stat.dmc_interrupt = 1;
		}

		/* Update status register */
		apu->r.stat.dmc_active = (apu->dmc.byte_count != 0);
	}

	/* At any time, if the interrupt flag is set, the CPU's IRQ line is
	continuously asserted until the interrupt flag is cleared. */
	if (apu->r.stat.dmc_interrupt)
		cpu_interrupt(apu->irq);

	/* Check if DMC channel needs update */
	if (apu->dmc.counter == 0) {
		/* If the silence flag is clear, the output level changes based
		on bit 0 of the shift register. If the bit is 1, add 2;
		otherwise, subtract 2. But if adding or subtracting 2 would
		cause the output level to leave the 0-127 range, leave the
		output level unchanged. */
		if (!apu->dmc.silenced) {
			inc = (bitops_getb(&apu->dmc.shift_reg, 0, 1) == 1);
			target = apu->r.dmc_load.value + (inc ? 2 : -2);
			if ((target >= 0) && (target <= 127))
				apu->r.dmc_load.value = target;
		}

		/* The right shift register is clocked. */
		apu->dmc.shift_reg >>= 1;

		/* When an output cycle ends, a new cycle is started as follows:
		The bits-remaining counter is loaded with 8. If the sample
		buffer is empty, then the silence flag is set; otherwise, the
		silence flag is cleared and the sample buffer is emptied into
		the shift register. */
		if (apu->dmc.bits_remaining == 0) {
			apu->dmc.bits_remaining = 8;
			if (!apu->dmc.sample_buffer_full) {
				/* Disable channel */
				apu->dmc.silenced = true;
			} else {
				/* Enable channel and load shift register */
				apu->dmc.silenced = false;
				apu->dmc.shift_reg = apu->dmc.sample_buffer;
				apu->dmc.sample_buffer = 0;
				apu->dmc.sample_buffer_full = false;
			}
		}

		/* The bits-remaining counter is updated whenever the timer
		outputs a clock, regardless of whether a sample is currently
		playing. */
		apu->dmc.bits_remaining--;

		/* Reload counter based on rate index */
		apu->dmc.counter = dmc_rate_table[apu->r.dmc_main.freq_id];
	}

	/* Decrement DMC channel counter */
	apu->dmc.counter--;
}

void apu_tick(struct apu *apu)
{
	float pulse1;
	float pulse2;
	float triangle;
	float noise;
	float dmc;
	float pulse_out;
	float tnd_out;
	float output;
	uint8_t buffer;

	/* The triangle channel's timer is clocked on every APU cycle, but the
	pulse, noise, and DMC timers are clocked only on every second APU cycle
	and thus produce only even periods. */
	triangle_update(apu);
	if (++apu->cycle == 2) {
		pulse_update(apu);
		noise_update(apu);
		apu->cycle = 0;
	}

	/* Update DMC channel */
	dmc_update(apu);

	/* Compute channel outputs */
	pulse1 = apu->pulse1.value * apu->pulse1.volume;
	pulse2 = apu->pulse2.value * apu->pulse2.volume;
	triangle = apu->triangle.value;
	noise = apu->noise.value * apu->noise.volume;
	dmc = apu->r.dmc_load.value;

	/* A linear approximation can be used, which results in slightly louder
	DMC samples, but otherwise fairly accurate operation since the wave
	channels use a small portion of the transfer curve. The overall volume
	will be reduced due to the headroom required by the DMC
	approximation. */
	pulse_out = 0.00752f * (pulse1 + pulse2);
	tnd_out = 0.00851f * triangle + 0.00494f * noise + 0.00335f * dmc;
	output = pulse_out + tnd_out;

	/* Enqueue audio data */
	buffer = output * UCHAR_MAX;
	audio_enqueue(&buffer, 1);

	/* Always consume one cycle */
	clock_consume(1);
}

void length_counter_tick(struct apu *apu)
{
	struct pulse *pulse;
	struct pulse_main *p_main;
	bool halt;
	int c;

	/* Handle pulse channels 1 & 2 */
	for (c = 1; c <= 2; c++) {
		/* Get appropriate data/registers */
		pulse = (c == 1) ? &apu->pulse1 : &apu->pulse2;
		p_main = (c == 1) ? &apu->r.pulse1_main : &apu->r.pulse2_main;

		/* Decrement length counter, silencing channel if needed */
		halt = p_main->env_loop_len_counter_halt;
		if (!halt && (pulse->len_counter != 0))
			if (--pulse->len_counter == 0)
				pulse->len_counter_silenced = true;
	}

	/* Decrement triangle length counter, silencing channel if needed */
	halt = apu->r.triangle_linear_counter.control_len_counter_halt;
	if (!halt && (apu->triangle.len_counter != 0))
		if (--apu->triangle.len_counter == 0)
			apu->triangle.len_counter_silenced = true;

	/* Decrement noise length counter, silencing channel if needed */
	halt = apu->r.noise_main.env_loop_len_counter_halt;
	if (!halt && (apu->noise.len_counter != 0))
		if (--apu->noise.len_counter == 0)
			apu->noise.len_counter_silenced = true;

	/* Update length counters status */
	apu->r.stat.pulse1_len_counter_stat = (apu->pulse1.len_counter > 0);
	apu->r.stat.pulse2_len_counter_stat = (apu->pulse2.len_counter > 0);
	apu->r.stat.triangle_len_counter_stat = (apu->triangle.len_counter > 0);
	apu->r.stat.noise_len_counter_stat = (apu->noise.len_counter > 0);
}

void vol_env_tick(struct apu *apu)
{
	struct pulse *pulse;
	struct pulse_main *p_main;
	int c;

	/* Handle pulse channels 1 & 2 */
	for (c = 1; c <= 2; c++) {
		/* Get appropriate data/registers */
		pulse = (c == 1) ? &apu->pulse1 : &apu->pulse2;
		p_main = (c == 1) ? &apu->r.pulse1_main : &apu->r.pulse2_main;

		/* Check if envelope start flag is set */
		if (!pulse->env_start) {
			/* Clock divider */
			if (pulse->env_period != 0) {
				/* Decrement divider period */
				pulse->env_period--;
			} else {
				/* Reload divider period */
				pulse->env_period = p_main->vol_env;

				/* If the counter is non-zero, it is
				decremented, otherwise if the loop flag is set,
				the counter is loaded with 15. */
				if (pulse->env_counter != 0)
					pulse->env_counter--;
				else if (p_main->env_loop_len_counter_halt)
					pulse->env_counter = 15;
			}
		} else {
			/* The start flag is cleared, the counter is loaded with
			15, and the divider's period is immediately reloaded. */
			pulse->env_start = false;
			pulse->env_counter = 15;
			pulse->env_period = p_main->vol_env;
		}

		/* The envelope unit's volume output depends on the constant
		volume flag: if set, the envelope parameter directly sets the
		volume, otherwise the counter's value is the current volume. The
		constant volume flag has no effect besides selecting the volume
		source; the envelope counter will still be updated when constant
		volume is selected. */
		if (p_main->constant_vol)
			pulse->volume = p_main->vol_env;
		else
			pulse->volume = pulse->env_counter;
	}

	/* Check if noise envelope start flag is set */
	if (!apu->noise.env_start) {
		/* Clock divider */
		if (apu->noise.env_period != 0) {
			/* Decrement divider period */
			apu->noise.env_period--;
		} else {
			/* Reload divider period */
			apu->noise.env_period = apu->r.noise_main.vol_env;

			/* If the counter is non-zero, it is
			decremented, otherwise if the loop flag is set,
			the counter is loaded with 15. */
			if (apu->noise.env_counter != 0)
				apu->noise.env_counter--;
			else if (apu->r.noise_main.env_loop_len_counter_halt)
				apu->noise.env_counter = 15;
		}
	} else {
		/* The start flag is cleared, the counter is loaded with
		15, and the divider's period is immediately reloaded. */
		apu->noise.env_start = false;
		apu->noise.env_counter = 15;
		apu->noise.env_period = apu->r.noise_main.vol_env;
	}

	/* The envelope unit's volume output depends on the constant
	volume flag: if set, the envelope parameter directly sets the
	volume, otherwise the counter's value is the current volume. The
	constant volume flag has no effect besides selecting the volume
	source; the envelope counter will still be updated when constant
	volume is selected. */
	if (apu->r.noise_main.constant_vol)
		apu->noise.volume = apu->r.noise_main.vol_env;
	else
		apu->noise.volume = apu->noise.env_counter;
}

void sweep_tick(struct apu *apu)
{
	struct pulse *pulse;
	struct pulse_sweep *sweep;
	struct pulse_timer_high *t_hi;
	uint8_t *t_lo;
	uint8_t counter;
	uint16_t current_period;
	uint16_t target_period;
	uint16_t res;
	bool reload;
	bool adjust_period;
	bool silenced;
	int c;

	/* Handle pulse channels 1 & 2 */
	for (c = 1; c <= 2; c++) {
		/* Get appropriate data/registers */
		pulse = (c == 1) ? &apu->pulse1 : &apu->pulse2;
		sweep = (c == 1) ? &apu->r.pulse1_sweep : &apu->r.pulse2_sweep;
		t_lo = (c == 1) ? &apu->r.pulse1_t_lo : &apu->r.pulse2_t_lo;
		t_hi = (c == 1) ? &apu->r.pulse1_t_hi : &apu->r.pulse2_t_hi;

		/* Initialize current pulse data */
		reload = pulse->sweep_reload;
		counter = pulse->sweep_counter;
		adjust_period = false;

		/* If the reload flag is set, the divider's counter is set to
		the period P. If the divider's counter was zero before the
		reload and the sweep is enabled, the pulse's period is also
		adjusted. The reload flag is then cleared. */
		if (reload) {
			pulse->sweep_counter = sweep->period;
			if ((counter == 0) && sweep->enabled)
				adjust_period = true;
			pulse->sweep_reload = false;
		}

		/* If the reload flag is clear and the divider's counter is
		non-zero, it is decremented. */
		if (!reload && (counter != 0))
			pulse->sweep_counter--;

		/* If the reload flag is clear and the divider's counter is zero
		and the sweep is enabled, the counter is set to P and the
		pulse's period is adjusted. */
		if (!reload && (counter == 0) && sweep->enabled) {
			pulse->sweep_counter = sweep->period;
			adjust_period = true;
		}

		/* Get current period, shift result, and target period */
		current_period = *t_lo | (t_hi->timer_high << 8);
		res = current_period >> sweep->shift;
		target_period = current_period + (sweep->negate ? -res : res);

		/* For reasons unknown, pulse channel 1 hardwires its adder's
		carry input rather than using the state of the negate flag,
		resulting in the subtraction operation adding the one's
		complement instead of the expected two's complement (as pulse
		channel 2 does). As a result, a negative sweep on pulse channel
		1 will subtract the shifted period value minus 1. */
		if ((c == 1) && sweep->negate)
			target_period++;

		/* When the channel's current period is less than 8 or the
		target period is greater than 0x7FF, the channel is silenced (0
		is sent to the mixer). */
		silenced = sweep->enabled;
		silenced &= ((current_period < 8) || (target_period > 0x7FF));
		pulse->sweep_silenced = silenced;

		/* Otherwise, if the enable flag is set and the shift count is
		non-zero, when the divider outputs a clock, the channel's period
		is updated. */
		adjust_period &= !silenced;
		adjust_period &= sweep->enabled;
		adjust_period &= (sweep->shift != 0);
		if (adjust_period) {
			/* Update channel period */
			*t_lo = target_period & 0xFF;
			t_hi->timer_high = target_period >> 8;
		}
	}
}

void linear_counter_tick(struct apu *apu)
{
	uint8_t counter;
	bool silenced;

	/* If the linear counter reload flag is set, the linear counter is
	reloaded with the counter reload value, otherwise if the linear counter
	is non-zero, it is decremented. */
	if (apu->triangle.linear_counter_reload) {
		/* Reload counter */
		counter = apu->r.triangle_linear_counter.reload_val;
		apu->triangle.linear_counter = counter;
	} else if (apu->triangle.linear_counter != 0) {
		/* Decrement counter */
		apu->triangle.linear_counter--;
	}

	/* If the control flag is clear, the linear counter reload flag is
	cleared. */
	if (!apu->r.triangle_linear_counter.control_len_counter_halt)
		apu->triangle.linear_counter_reload = 0;

	/* Silence triangle channel if linear counter is 0 */
	silenced = (apu->triangle.linear_counter == 0);
	apu->triangle.linear_counter_silenced = silenced;
}

void seq_tick(struct apu *apu)
{
	int s;
	int num_steps;
	bool f;
	bool l;
	bool e;

	/* Get current frame sequencer step */
	s = apu->seq_step;

	/* Get number of steps based on mode - if the mode flag is clear, the
	4-step sequence is selected, otherwise the 5-step sequence is
	selected. */
	num_steps = (apu->r.seq.mode == 0) ? 4 : 5;

	/* The frame sequencer contains a divider and a sequencer which clocks
	various units.
	mode 0: 4-step  effective rate (approx)
	---------------------------------------
	    - - - f      60 Hz
	    - l - l     120 Hz
	    e e e e     240 Hz

	mode 1: 5-step  effective rate (approx)
	---------------------------------------
	    - - - - -   (interrupt flag never set)
	    l - l - -    96 Hz
	    e e e e -   192 Hz */
	switch (apu->r.seq.mode) {
	case 0:
		f = (s == 3);
		l = ((s == 1) || (s == 3));
		e = true;
		break;
	case 1:
	default:
		f = false;
		l = ((s == 0) || (s == 2));
		e = (s <= 3);
		break;
	}

	/* Increment sequencer step and handle overflow */
	if (++apu->seq_step == num_steps)
		apu->seq_step = 0;

	/* The frame interrupt flag is set at a particular point in the 4-step
	sequence provided the interrupt inhibit flag is clear. */
	if (f && !apu->r.seq.int_inhibit)
		apu->r.stat.frame_interrupt = 1;

	/* The frame interrupt flag is connected to the CPU's IRQ line. */
	if (apu->r.stat.frame_interrupt)
		cpu_interrupt(apu->irq);

	/* Check for length counter and sweep event */
	if (l) {
		/* Clock length counters and sweep units */
		length_counter_tick(apu);
		sweep_tick(apu);
	}

	/* Clock envelopes and linear counter if required */
	if (e) {
		vol_env_tick(apu);
		linear_counter_tick(apu);
	}

	/* Always consume one cycle */
	clock_consume(1);
}

bool apu_init(struct controller_instance *instance)
{
	struct apu *apu;
	struct audio_specs audio_specs;
	struct resource *res;

	/* Allocate APU structure */
	instance->priv_data = calloc(1, sizeof(struct apu));
	apu = instance->priv_data;

	/* Save bus ID */
	apu->bus_id = instance->bus_id;

	/* Add main memory region */
	res = resource_get("main",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	apu->main_region.area = res;
	apu->main_region.mops = &apu_mops;
	apu->main_region.data = apu;
	memory_region_add(&apu->main_region);

	/* Add control/status region */
	res = resource_get("ctrl_stat",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	apu->ctrl_stat_region.area = res;
	apu->ctrl_stat_region.mops = &ctrl_stat_mops;
	apu->ctrl_stat_region.data = apu;
	memory_region_add(&apu->ctrl_stat_region);

	/* Add frame counter region */
	res = resource_get("seq",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	apu->seq_region.area = res;
	apu->seq_region.mops = &seq_mops;
	apu->seq_region.data = apu;
	memory_region_add(&apu->seq_region);

	/* Add main clock */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	apu->main_clock.rate = res->data.clk;
	apu->main_clock.data = apu;
	apu->main_clock.tick = (clock_tick_t)apu_tick;
	apu->main_clock.enabled = true;
	clock_add(&apu->main_clock);

	/* Add frame sequencer clock */
	res = resource_get("seq_clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	apu->seq_clock.rate = res->data.clk;
	apu->seq_clock.data = apu;
	apu->seq_clock.tick = (clock_tick_t)seq_tick;
	apu->seq_clock.enabled = true;
	clock_add(&apu->seq_clock);

	/* Save IRQ number */
	res = resource_get("irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	apu->irq = res->data.irq;

	/* Initialize audio frontend */
	audio_specs.freq = apu->main_clock.rate;
	audio_specs.format = AUDIO_FORMAT_U8;
	audio_specs.channels = 1;
	if (!audio_init(&audio_specs)) {
		free(apu);
		return false;
	}

	return true;
}

void apu_reset(struct controller_instance *instance)
{
	struct apu *apu = instance->priv_data;

	/* Initialize controller data */
	memset(&apu->r, 0, sizeof(struct apu_regs));
	memset(&apu->pulse1, 0, sizeof(struct pulse));
	memset(&apu->pulse2, 0, sizeof(struct pulse));
	memset(&apu->triangle, 0, sizeof(struct triangle));
	memset(&apu->noise, 0, sizeof(struct noise));
	apu->noise.shift_reg = 1;
	apu->seq_step = 0;
	apu->cycle = 0;

	/* Silence all channels */
	apu->pulse1.len_counter_silenced = true;
	apu->pulse1.sweep_silenced = true;
	apu->pulse2.len_counter_silenced = true;
	apu->pulse2.sweep_silenced = true;
	apu->triangle.len_counter_silenced = true;
	apu->triangle.linear_counter_silenced = true;
	apu->noise.len_counter_silenced = true;
}

void apu_deinit(struct controller_instance *instance)
{
	audio_deinit();
	free(instance->priv_data);
}

CONTROLLER_START(apu)
	.init = apu_init,
	.reset = apu_reset,
	.deinit = apu_deinit
CONTROLLER_END

