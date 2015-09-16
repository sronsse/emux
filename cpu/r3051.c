#include <stdlib.h>
#include <string.h>
#include <clock.h>
#include <cpu.h>
#include <log.h>
#include <memory.h>
#include <resource.h>

#define NUM_REGISTERS			32
#define NUM_COP0_REGISTERS		64
#define NUM_COP2_DATA_REGISTERS		32
#define NUM_COP2_CTRL_REGISTERS		32
#define INITIAL_PC			0xBFC00000
#define NUM_CACHE_LINES			256
#define NUM_INSTRUCTIONS_PER_CACHE_LINE	4

#define KUSEG_START			0x00000000
#define KUSEG_END			(KUSEG_START + MB(2048) - 1)
#define KSEG0_START			0x80000000
#define KSEG0_END			(KSEG0_START + MB(512) - 1)
#define KSEG1_START			0xA0000000
#define KSEG1_END			(KSEG1_START + MB(512) - 1)
#define KSEG2_START			0xC0000000
#define KSEG2_END			(KSEG2_START + MB(1024) - 1)

#define WITHIN_REGION(a, region) ((a >= region##_START) && (a <= region##_END))
#define PHYSICAL_ADDRESS(a) (bitops_getl(&a, 0, 29))

union address {
	uint32_t raw;
	struct {
		uint32_t word_alignment:2;
		uint32_t index:2;
		uint32_t cache_line:8;
		uint32_t tag:19;
	};
};

struct cached_instruction {
	uint32_t value;
	bool valid;
};

struct cache_line {
	uint32_t tag;
	struct cached_instruction instructions[NUM_INSTRUCTIONS_PER_CACHE_LINE];
};

union cache_control {
	uint32_t raw;
	struct {
		uint32_t LOCK:1;	/* Lock Mode */
		uint32_t INV:1;		/* Invalidate Mode */
		uint32_t TAG:1;		/* Tag Test Mode */
		uint32_t RAM:1;		/* Scratchpad RAM */
		uint32_t DBLKSZ:2;	/* D-Cache Refill Size */
		uint32_t reserved1:1;
		uint32_t DS:1;		/* Enable D-Cache */
		uint32_t IBLKSZ:2;	/* I-Cache Refill Size */
		uint32_t IS0:1;		/* Enable I-Cache Set 0 */
		uint32_t IS1:1;		/* Enable I-Cache Set 1 */
		uint32_t INTP:1;	/* Interrupt Polarity */
		uint32_t RDPRI:1;	/* Enable Read Priority */
		uint32_t NOPAD:1;	/* No Wait State */
		uint32_t BGNT:1;	/* Enable Bus Grant */
		uint32_t LDSCH:1;	/* Enable Load Scheduling */
		uint32_t NOSTR:1;	/* No Streaming */
		uint32_t reserved2:14;
	};
};

/* I-Type (Immediate) instruction */
struct i_type {
	uint32_t immediate:16;
	uint32_t rt:5;
	uint32_t rs:5;
	uint32_t opcode:6;
};

/* J-Type (Jump) instruction */
struct j_type {
	uint32_t target:26;
	uint32_t opcode:6;
};

/* R-Type (Register) instruction */
struct r_type {
	uint32_t funct:6;
	uint32_t shamt:5;
	uint32_t rd:5;
	uint32_t rt:5;
	uint32_t rs:5;
	uint32_t opcode:6;
};

/* Special instruction */
struct special {
	uint32_t opcode:6;
	uint32_t reserved:26;
};

/* Branch condition instruction */
struct bcond {
	uint32_t reserved:16;
	uint32_t opcode:5;
	uint32_t reserved2:11;
};

/* Coprocessor instruction */
struct cop_instr {
	uint32_t reserved:21;
	uint32_t opcode:5;
	uint32_t reserved2:6;
};

/* Coprocessor 2 instruction */
struct cop2_instr {
	uint32_t real_cmd:6;
	uint32_t reserved:4;
	uint32_t lm:1;
	uint32_t reserved2:2;
	uint32_t mvmva_translation_vector:2;
	uint32_t mvmva_multiply_vector:2;
	uint32_t mvmva_multiply_matrix:2;
	uint32_t sf:1;
	uint32_t fake_cmd:5;
	uint32_t imm25:7;
};

union instruction {
	uint32_t raw;
	struct {
		uint32_t reserved:26;
		uint32_t opcode:6;
	};
	struct i_type i_type;
	struct j_type j_type;
	struct r_type r_type;
	struct special special;
	struct bcond bcond;
	struct cop_instr cop;
	struct cop2_instr cop2;
};

union cop0_stat {
	uint32_t raw;
	struct {
		uint32_t IEc:1;		/* Interrupt Enable (current) */
		uint32_t KUc:1;		/* Kernel-User Mode (current) */
		uint32_t IEp:1;		/* Interrupt Enable (previous) */
		uint32_t KUp:1;		/* Kernel-User Mode (previous) */
		uint32_t IEo:1;		/* Interrupt Enable (old) */
		uint32_t KUo:1;		/* Kernel-User Mode (old) */
		uint32_t reserved1:2;
		uint32_t Sw:2;		/* Software Interrupt Mask */
		uint32_t Intr:6;	/* Hardware Interrupt Mask */
		uint32_t IsC:1;		/* Isolate Cache */
		uint32_t reserved2:1;
		uint32_t PZ:1;		/* Parity Zero */
		uint32_t reserved3:1;
		uint32_t PE:1;		/* Parity Error */
		uint32_t TS:1;		/* TLB Shutdown */
		uint32_t BEV:1;		/* Bootstrap Exception Vectors */
		uint32_t reserved4:5;
		uint32_t Cu:4;		/* Coprocessor Usability */
	};
};

union cop0_cause {
	uint32_t raw;
	struct {
		uint32_t reserved1:2;
		uint32_t ExcCode:5;	/* Exception Code */
		uint32_t reserved2:1;
		uint32_t Sw:2;		/* Software Interrupts */
		uint32_t IP:6;		/* Interrupt Pending */
		uint32_t reserved3:12;
		uint32_t CE:2;		/* Coprocessor Error */
		uint32_t BT:1;		/* Branch Taken */
		uint32_t BD:1;		/* Branch Delay */
	};
};

union cop0 {
	uint32_t R[NUM_COP0_REGISTERS];
	struct {
		uint32_t r0;
		uint32_t r1;
		uint32_t r2;
		uint32_t BPC;		/* Breakpoint Program Counter */
		uint32_t r4;
		uint32_t BDA;		/* Breakpoint Data Address */
		uint32_t TAR;		/* Target Address */
		uint32_t DCIC;		/* Debug and Cache Invalidate Control */
		uint32_t BadA;		/* Bad Address */
		uint32_t BDAM;		/* Breakpoint Data Address Mask */
		uint32_t r10;
		uint32_t BPCM;		/* Breakpoint Program Counter Mask */
		union cop0_stat stat;	/* Status */
		union cop0_cause cause;	/* Cause */
		uint32_t EPC;		/* Exception Program Counter */
		uint32_t PRId;		/* Processor Revision Identifier */
		uint32_t reserved[32];
	};
};

union cop2_vec16 {
	int16_t data[3];
	struct {
		int16_t X;
		int16_t Y;
		int16_t Z;
		uint16_t unused;
	};
};

union cop2_vec32 {
	int32_t data[3];
	struct {
		int32_t X;
		int32_t Y;
		int32_t Z;
	};
	struct {
		uint32_t R;
		uint32_t G;
		uint32_t B;
	};
};

union cop2_matrix {
	int16_t data[3][3];
	struct {
		int16_t _11;
		int16_t _12;
		int16_t _13;
		int16_t _21;
		int16_t _22;
		int16_t _23;
		int16_t _31;
		int16_t _32;
		int16_t _33;
		uint16_t reserved;
	};
};

union cop2_rgbc {
	uint8_t data[4];
	struct {
		uint8_t R;
		uint8_t G;
		uint8_t B;
		uint8_t C;
	};
};

struct cop2_ir {
	int16_t data;
	uint16_t reserved;
};

union cop2_sxy {
	int16_t data[2];
	struct {
		int16_t X;
		int16_t Y;
	};
};

struct cop2_sz {
	uint16_t Z;
	uint16_t reserved;
};

union cop2_flag {
	uint32_t raw;
	struct {
		uint32_t unused:12;
		uint32_t ir0_sat:1;
		uint32_t sy2_sat:1;
		uint32_t sx2_sat:1;
		uint32_t mac0_larger_neg:1;
		uint32_t mac0_larger_pos:1;
		uint32_t div_overflow:1;
		uint32_t sz3_otz_sat:1;
		uint32_t b_sat:1;
		uint32_t g_sat:1;
		uint32_t r_sat:1;
		uint32_t ir3_sat:1;
		uint32_t ir2_sat:1;
		uint32_t ir1_sat:1;
		uint32_t mac3_larger_neg:1;
		uint32_t mac2_larger_neg:1;
		uint32_t mac1_larger_neg:1;
		uint32_t mac3_larger_pos:1;
		uint32_t mac2_larger_pos:1;
		uint32_t mac1_larger_pos:1;
		uint32_t error:1;
	};
};

union cop2 {
	struct {
		uint32_t DR[NUM_COP2_DATA_REGISTERS];
		uint32_t CR[NUM_COP2_CTRL_REGISTERS];
	};
	struct {
		union cop2_vec16 V[3];
		union cop2_rgbc RGBC;
		uint16_t OTZ;
		uint16_t reserved1;
		struct cop2_ir IR[4];
		union cop2_sxy SXY[4];
		struct cop2_sz SZ[4];
		union cop2_rgbc RGB_FIFO[3];
		uint32_t reserved2;
		int32_t MAC[4];
		uint32_t IRGB:15;
		uint32_t reserved3:17;
		uint32_t ORGB:15;
		uint32_t reserved4:17;
		int32_t LZCS;
		int32_t LZCR;
		union cop2_matrix RT;
		union cop2_vec32 TR;
		union cop2_matrix LLM;
		union cop2_vec32 BK;
		union cop2_matrix LCM;
		union cop2_vec32 FC;
		int32_t OFX;
		int32_t OFY;
		uint16_t H;
		uint16_t reserved5;
		int16_t DQA;
		uint16_t reserved6;
		int32_t DQB;
		int16_t ZSF3;
		uint16_t reserved7;
		int16_t ZSF4;
		uint16_t reserved8;
		union cop2_flag FLAG;
	};
};

struct branch_delay {
	uint32_t PC;
	bool delay;
	bool pending;
};

struct r3051 {
	union {
		uint32_t R[NUM_REGISTERS];
		struct {
			uint32_t zr;
			uint32_t at;
			uint32_t v0;
			uint32_t v1;
			uint32_t a0;
			uint32_t a1;
			uint32_t a2;
			uint32_t a3;
			uint32_t t0;
			uint32_t t1;
			uint32_t t2;
			uint32_t t3;
			uint32_t t4;
			uint32_t t5;
			uint32_t t6;
			uint32_t t7;
			uint32_t s0;
			uint32_t s1;
			uint32_t s2;
			uint32_t s3;
			uint32_t s4;
			uint32_t s5;
			uint32_t s6;
			uint32_t s7;
			uint32_t t8;
			uint32_t t9;
			uint32_t k0;
			uint32_t k1;
			uint32_t gp;
			uint32_t sp;
			uint32_t fp;
			uint32_t ra;
		};
	};
	uint32_t HI;
	uint32_t LO;
	uint32_t PC;
	struct branch_delay branch_delay;
	union instruction instruction;
	union cop0 cop0;
	union cop2 cop2;
	union cache_control cache_ctrl;
	struct cache_line instruction_cache[NUM_CACHE_LINES];
	int bus_id;
	struct clock clock;
	struct region cache_ctrl_region;
};

static bool r3051_init(struct cpu_instance *instance);
static void r3051_reset(struct cpu_instance *instance);
static void r3051_deinit(struct cpu_instance *instance);
static void r3051_fetch(struct r3051 *cpu);
static void r3051_branch(struct r3051 *cpu, uint32_t PC);
static void r3051_tick(struct r3051 *cpu);

#define DEFINE_MEM_READ(ext, type) \
	static type mem_read##ext(struct r3051 *cpu, address_t a) \
	{ \
		/* Translate to physical address (strip leading 3 bits) */ \
		if (!WITHIN_REGION(a, KSEG2)) \
			a = PHYSICAL_ADDRESS(a); \
	\
		/* Call regular memory operation */ \
		return memory_read##ext(cpu->bus_id, a); \
	}

#define DEFINE_MEM_WRITE(ext, type) \
	static void mem_write##ext(struct r3051 *cpu, type data, address_t a) \
	{ \
		union address address; \
		struct cache_line *line; \
		int i; \
	\
		/* Translate to physical address (strip leading 3 bits) */ \
		if (!WITHIN_REGION(a, KSEG2)) \
			a = PHYSICAL_ADDRESS(a); \
	\
		/* Call regular memory operation if cache is not isolated */ \
		if (!cpu->cop0.stat.IsC) { \
			memory_write##ext(cpu->bus_id, data, a); \
			return; \
		} \
	\
		/* Get requested cache line */ \
		address.raw = a; \
		line = &cpu->instruction_cache[address.cache_line]; \
	\
		/* Invalidate instruction cache if TAG TEST mode is enabled */ \
		if (cpu->cache_ctrl.TAG) { \
			for (i = 0; i < NUM_INSTRUCTIONS_PER_CACHE_LINE; i++) \
				line->instructions[i].valid = false; \
			return; \
		} \
	\
		/* Fill instruction cache with input data */ \
		line->instructions[address.index].value = data; \
	}

DEFINE_MEM_READ(b, uint8_t)
DEFINE_MEM_WRITE(b, uint8_t)
DEFINE_MEM_READ(w, uint16_t)
DEFINE_MEM_WRITE(w, uint16_t)
DEFINE_MEM_READ(l, uint32_t)
DEFINE_MEM_WRITE(l, uint32_t)

void r3051_fetch(struct r3051 *cpu)
{
	bool cache_access;
	union address address;
	struct cache_line *line;
	bool valid_instruction;
	uint32_t a;
	int i;

	/* Set address to fetch and translate it */
	a = PHYSICAL_ADDRESS(cpu->PC);

	/* Check if cache is enabled or needed (KSEG1 access is uncached) */
	cache_access = cpu->cache_ctrl.IS1;
	cache_access &= !WITHIN_REGION(cpu->PC, KSEG1);

	/* Fetch instruction and return if no cache access is needed */
	if (!cache_access) {
		cpu->instruction.raw = memory_readl(cpu->bus_id, a);
		cpu->PC += 4;
		clock_consume(4);
		return;
	}

	/* Get requested cache line */
	address.raw = a;
	line = &cpu->instruction_cache[address.cache_line];

	/* Check if instruction within cache is valid */
	valid_instruction = (line->tag == address.tag);
	valid_instruction &= line->instructions[address.index].valid;

	/* Handle cache hit */
	if (valid_instruction) {
		cpu->instruction.raw = line->instructions[address.index].value;
		cpu->PC += 4;
		return;
	}

	/* Invalidate instructions prior to requested index */
	for (i = 0; i < address.index; i++)
		line->instructions[i].valid = false;

	/* Fill cache by fetching instructions from requested index */
	for (i = address.index; i < NUM_INSTRUCTIONS_PER_CACHE_LINE; i++) {
		line->instructions[i].value = memory_readl(cpu->bus_id, a);
		line->instructions[i].valid = true;
		a += 4;
		clock_consume(1);
	}

	/* Update cache line tag */
	line->tag = address.tag;

	/* Fill instruction from cache (which is now valid) */
	cpu->instruction.raw = line->instructions[address.index].value;
	cpu->PC += 4;
	clock_consume(3);
}

void r3051_branch(struct r3051 *cpu, uint32_t PC)
{
	/* Discard branch if one is pending already */
	if (cpu->branch_delay.delay)
		return;

	/* Set branch delay PC and pending flag */
	cpu->branch_delay.PC = PC;
	cpu->branch_delay.pending = true;
}

void r3051_tick(struct r3051 *cpu)
{
	/* Fetch instruction */
	r3051_fetch(cpu);

	/* Handle pending branch delay (branch was taken in previous cycle) */
	if (cpu->branch_delay.pending) {
		cpu->branch_delay.delay = true;
		cpu->branch_delay.pending = false;
	}

	/* Execute instruction */
	switch (cpu->instruction.opcode) {
	default:
		LOG_W("Unknown instruction (%08x)!\n", cpu->instruction.raw);
		break;
	}

	/* Handle branch delay (PC now needs to be updated) */
	if (cpu->branch_delay.delay) {
		cpu->PC = cpu->branch_delay.PC;
		cpu->branch_delay.delay = false;
	}

	/* Always consume one cycle */
	clock_consume(1);
}

bool r3051_init(struct cpu_instance *instance)
{
	struct r3051 *cpu;
	struct resource *res;

	/* Allocate r3051 structure and set private data */
	cpu = calloc(1, sizeof(struct r3051));
	instance->priv_data = cpu;

	/* Save bus ID */
	cpu->bus_id = instance->bus_id;

	/* Add CPU clock */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	cpu->clock.rate = res->data.clk;
	cpu->clock.data = cpu;
	cpu->clock.tick = (clock_tick_t)r3051_tick;
	clock_add(&cpu->clock);

	/* Add cache control region */
	res = resource_get("cache_control",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	cpu->cache_ctrl_region.area = res;
	cpu->cache_ctrl_region.mops = &ram_mops;
	cpu->cache_ctrl_region.data = &cpu->cache_ctrl.raw;
	memory_region_add(&cpu->cache_ctrl_region);

	return true;
}

void r3051_reset(struct cpu_instance *instance)
{
	struct r3051 *cpu = instance->priv_data;

	/* Intialize processor data */
	cpu->PC = INITIAL_PC;
	memset(cpu->R, 0, NUM_REGISTERS * sizeof(uint32_t));
	memset(cpu->cop0.R, 0, NUM_COP0_REGISTERS * sizeof(uint32_t));
	memset(cpu->cop2.DR, 0, NUM_COP2_DATA_REGISTERS * sizeof(uint32_t));
	memset(cpu->cop2.CR, 0, NUM_COP2_CTRL_REGISTERS * sizeof(uint32_t));
	cpu->branch_delay.delay = false;
	cpu->branch_delay.pending = false;

	/* Enable clock */
	cpu->clock.enabled = true;
}

void r3051_deinit(struct cpu_instance *instance)
{
	free(instance->priv_data);
}

CPU_START(r3051)
	.init = r3051_init,
	.reset = r3051_reset,
	.deinit = r3051_deinit
CPU_END

