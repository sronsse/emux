#include <stdlib.h>
#include <string.h>
#include <bitops.h>
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
#define EXCEPTION_ADDR_0		0x80000080
#define EXCEPTION_ADDR_1		0xBFC00180
#define NUM_CACHE_LINES			256
#define NUM_INSTRUCTIONS_PER_CACHE_LINE	4

#define I_STAT				0x00
#define I_MASK				0x04

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

enum exception {
	EXCEPTION_Int,	/* External Interrupt */
	EXCEPTION_Res1,
	EXCEPTION_Res2,
	EXCEPTION_Res3,
	EXCEPTION_AdEL,	/* Address Error Exception (load instruction) */
	EXCEPTION_AdES,	/* Address Error Exception (store instruction) */
	EXCEPTION_IBE,	/* Bus Error Exception (for an instruction fetch) */
	EXCEPTION_DBE,	/* Bus Error Exception (for a data load or store) */
	EXCEPTION_Sys,	/* SYSCALL Exception */
	EXCEPTION_Bp,	/* Breakpoint Exception */
	EXCEPTION_RI,	/* Reserved Instruction Exception */
	EXCEPTION_CpU,	/* Coprocessor Unusable Exception */
	EXCEPTION_Ovf,	/* Arithmetic Overflow Exception */
	EXCEPTION_ResD,
	EXCEPTION_ResE,
	EXCEPTION_ResF
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

struct load_delay {
	uint8_t reg;
	uint32_t data;
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
	uint32_t current_PC;
	struct branch_delay branch_delay;
	struct load_delay load_delay;
	union instruction instruction;
	union cop0 cop0;
	union cop2 cop2;
	uint32_t i_stat;
	uint32_t i_mask;
	union cache_control cache_ctrl;
	struct cache_line instruction_cache[NUM_CACHE_LINES];
	int bus_id;
	struct clock clock;
	struct region int_ctrl_region;
	struct region cache_ctrl_region;
};

static bool r3051_init(struct cpu_instance *instance);
static void r3051_reset(struct cpu_instance *instance);
static void r3051_interrupt(struct cpu_instance *instance, int irq);
static void r3051_deinit(struct cpu_instance *instance);
static uint16_t r3051_int_ctrl_readw(struct r3051 *cpu, uint32_t a);
static uint32_t r3051_int_ctrl_readl(struct r3051 *cpu, uint32_t a);
static void r3051_int_ctrl_writew(struct r3051 *cpu, uint16_t w, uint32_t a);
static void r3051_int_ctrl_writel(struct r3051 *cpu, uint32_t l, uint32_t a);
static void r3051_fetch(struct r3051 *cpu);
static void r3051_handle_interrupts(struct r3051 *cpu);
static void r3051_branch(struct r3051 *cpu, uint32_t PC);
static void r3051_load(struct r3051 *cpu, uint8_t reg, uint32_t data);
static void r3051_set(struct r3051 *cpu, uint8_t reg, uint32_t data);
static void r3051_tick(struct r3051 *cpu);
static void r3051_opcode_SPECIAL(struct r3051 *cpu);
static void r3051_opcode_BCOND(struct r3051 *cpu);
static void r3051_opcode_COP0(struct r3051 *cpu);
static void r3051_raise_exception(struct r3051 *cpu, enum exception e);

static inline void LB(struct r3051 *cpu);
static inline void LBU(struct r3051 *cpu);
static inline void LH(struct r3051 *cpu);
static inline void LHU(struct r3051 *cpu);
static inline void LW(struct r3051 *cpu);
static inline void LWL(struct r3051 *cpu);
static inline void LWR(struct r3051 *cpu);
static inline void SB(struct r3051 *cpu);
static inline void SH(struct r3051 *cpu);
static inline void SW(struct r3051 *cpu);
static inline void SWL(struct r3051 *cpu);
static inline void SWR(struct r3051 *cpu);
static inline void ADDI(struct r3051 *cpu);
static inline void ADDIU(struct r3051 *cpu);
static inline void SLTI(struct r3051 *cpu);
static inline void SLTIU(struct r3051 *cpu);
static inline void ANDI(struct r3051 *cpu);
static inline void ORI(struct r3051 *cpu);
static inline void XORI(struct r3051 *cpu);
static inline void LUI(struct r3051 *cpu);
static inline void ADD(struct r3051 *cpu);
static inline void ADDU(struct r3051 *cpu);
static inline void SUB(struct r3051 *cpu);
static inline void SUBU(struct r3051 *cpu);
static inline void SLT(struct r3051 *cpu);
static inline void SLTU(struct r3051 *cpu);
static inline void AND(struct r3051 *cpu);
static inline void OR(struct r3051 *cpu);
static inline void XOR(struct r3051 *cpu);
static inline void NOR(struct r3051 *cpu);
static inline void SLL(struct r3051 *cpu);
static inline void SRL(struct r3051 *cpu);
static inline void SRA(struct r3051 *cpu);
static inline void SLLV(struct r3051 *cpu);
static inline void SRLV(struct r3051 *cpu);
static inline void SRAV(struct r3051 *cpu);
static inline void MULT(struct r3051 *cpu);
static inline void MULTU(struct r3051 *cpu);
static inline void DIV(struct r3051 *cpu);
static inline void DIVU(struct r3051 *cpu);
static inline void MFHI(struct r3051 *cpu);
static inline void MFLO(struct r3051 *cpu);
static inline void MTHI(struct r3051 *cpu);
static inline void MTLO(struct r3051 *cpu);
static inline void J(struct r3051 *cpu);
static inline void JAL(struct r3051 *cpu);
static inline void JR(struct r3051 *cpu);
static inline void JALR(struct r3051 *cpu);
static inline void BEQ(struct r3051 *cpu);
static inline void BNE(struct r3051 *cpu);
static inline void BLEZ(struct r3051 *cpu);
static inline void BGTZ(struct r3051 *cpu);
static inline void BLTZ(struct r3051 *cpu);
static inline void BGEZ(struct r3051 *cpu);
static inline void BLTZAL(struct r3051 *cpu);
static inline void BGEZAL(struct r3051 *cpu);
static inline void SYSCALL(struct r3051 *cpu);
static inline void BREAK(struct r3051 *cpu);

static inline void MFC0(struct r3051 *cpu);
static inline void MTC0(struct r3051 *cpu);
static inline void RFE(struct r3051 *cpu);

static struct mops int_ctrl_mops = {
	.readw = (readw_t)r3051_int_ctrl_readw,
	.readl = (readl_t)r3051_int_ctrl_readl,
	.writew = (writew_t)r3051_int_ctrl_writew,
	.writel = (writel_t)r3051_int_ctrl_writel
};

uint16_t r3051_int_ctrl_readw(struct r3051 *cpu, uint32_t a)
{
	uint16_t w = 0;

	/* Read requested register */
	switch (a) {
	case I_STAT:
		w = cpu->i_stat;
		break;
	case I_MASK:
		w = cpu->i_mask;
		break;
	}

	return w;
}

uint32_t r3051_int_ctrl_readl(struct r3051 *cpu, uint32_t a)
{
	uint32_t l = 0;

	/* Read requested register */
	switch (a) {
	case I_STAT:
		l = cpu->i_stat;
		break;
	case I_MASK:
		l = cpu->i_mask;
		break;
	}

	return l;
}

void r3051_int_ctrl_writew(struct r3051 *cpu, uint16_t w, uint32_t a)
{
	/* Write requested register */
	switch (a) {
	case I_STAT:
		/* Acknowledge interrupt */
		cpu->i_stat &= w;
		break;
	case I_MASK:
		cpu->i_mask = w;
		break;
	}

	/* Update interrupt pending flag in cop0 cause register */
	if (cpu->cop0.cause.IP)
		cpu->cop0.cause.IP = ((cpu->i_stat & cpu->i_mask) != 0);
}

void r3051_int_ctrl_writel(struct r3051 *cpu, uint32_t l, uint32_t a)
{
	/* Write requested register */
	switch (a) {
	case I_STAT:
		/* Acknowledge interrupt */
		cpu->i_stat &= l;
		break;
	case I_MASK:
		cpu->i_mask = l;
		break;
	}

	/* Update interrupt pending flag in cop0 cause register */
	if (cpu->cop0.cause.IP)
		cpu->cop0.cause.IP = ((cpu->i_stat & cpu->i_mask) != 0);
}

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

void LB(struct r3051 *cpu)
{
	uint32_t a;
	int8_t b;
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;
	b = mem_readb(cpu, a);
	r3051_load(cpu, cpu->instruction.i_type.rt, b);
}

void LBU(struct r3051 *cpu)
{
	uint32_t a;
	uint8_t b;
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;
	b = mem_readb(cpu, a);
	r3051_load(cpu, cpu->instruction.i_type.rt, b);
}

void LH(struct r3051 *cpu)
{
	uint32_t a;
	int16_t w;
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;

	/* Handle alignment exception */
	if (a & 0x01) {
		r3051_raise_exception(cpu, EXCEPTION_AdEL);
		return;
	}

	w = mem_readw(cpu, a);
	r3051_load(cpu, cpu->instruction.i_type.rt, w);
}

void LHU(struct r3051 *cpu)
{
	uint32_t a;
	uint16_t w;
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;

	/* Handle alignment exception */
	if (a & 0x01) {
		r3051_raise_exception(cpu, EXCEPTION_AdEL);
		return;
	}

	w = mem_readw(cpu, a);
	r3051_load(cpu, cpu->instruction.i_type.rt, w);
}

void LW(struct r3051 *cpu)
{
	uint32_t a;
	uint32_t l;
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;

	/* Handle alignment exception */
	if (a & 0x03) {
		r3051_raise_exception(cpu, EXCEPTION_AdEL);
		return;
	}

	l = mem_readl(cpu, a);
	r3051_load(cpu, cpu->instruction.i_type.rt, l);
}

void LWL(struct r3051 *cpu)
{
	uint32_t a;
	uint32_t aligned_a;
	uint32_t l;
	uint32_t v;
	bool delay;

	/* Check if target register has a pending load delay */
	delay = cpu->load_delay.delay;
	delay &= (cpu->load_delay.reg == cpu->instruction.i_type.rt);

	/* Read value at 32-bit aligned address */
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;
	aligned_a = a;
	bitops_setl(&aligned_a, 0, 2, 0);
	l = mem_readl(cpu, aligned_a);

	/* Update target value */
	v = delay ? cpu->load_delay.data : cpu->R[cpu->instruction.i_type.rt];
	switch (bitops_getl(&a, 0, 2)) {
	case 0:
		bitops_setl(&v, 24, 8, l);
		break;
	case 1:
		bitops_setl(&v, 16, 16, l);
		break;
	case 2:
		bitops_setl(&v, 8, 24, l);
		break;
	case 3:
		v = l;
		break;
	}

	/* Load target register with updated value */
	r3051_load(cpu, cpu->instruction.i_type.rt, v);
}

void LWR(struct r3051 *cpu)
{
	uint32_t a;
	uint32_t aligned_a;
	uint32_t l;
	uint32_t v;
	bool delay;

	/* Check if target register has a pending load delay */
	delay = cpu->load_delay.delay;
	delay &= (cpu->load_delay.reg == cpu->instruction.i_type.rt);

	/* Read value at 32-bit aligned address */
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;
	aligned_a = a;
	bitops_setl(&aligned_a, 0, 2, 0);
	l = mem_readl(cpu, aligned_a);

	/* Update target value */
	v = delay ? cpu->load_delay.data : cpu->R[cpu->instruction.i_type.rt];
	switch (bitops_getl(&a, 0, 2)) {
	case 0:
		v = l;
		break;
	case 1:
		bitops_setl(&v, 0, 24, bitops_getl(&l, 8, 24));
		break;
	case 2:
		bitops_setl(&v, 0, 16, bitops_getl(&l, 16, 16));
		break;
	case 3:
		bitops_setl(&v, 0, 8, bitops_getl(&l, 24, 8));
		break;
	}

	/* Load target register with updated value */
	r3051_load(cpu, cpu->instruction.i_type.rt, v);
}

void SB(struct r3051 *cpu)
{
	uint32_t a;
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;
	mem_writeb(cpu, cpu->R[cpu->instruction.i_type.rt], a);
}

void SH(struct r3051 *cpu)
{
	uint32_t a;
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;

	/* Handle alignment exception */
	if (a & 0x01) {
		r3051_raise_exception(cpu, EXCEPTION_AdES);
		return;
	}

	mem_writew(cpu, cpu->R[cpu->instruction.i_type.rt], a);
}

void SW(struct r3051 *cpu)
{
	uint32_t a;
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;

	/* Handle alignment exception */
	if (a & 0x03) {
		r3051_raise_exception(cpu, EXCEPTION_AdES);
		return;
	}

	mem_writel(cpu, cpu->R[cpu->instruction.i_type.rt], a);
}

void SWL(struct r3051 *cpu)
{
	uint32_t a;
	uint32_t aligned_a;
	uint32_t l;
	uint32_t v;

	/* Read value at 32-bit aligned address */
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;
	aligned_a = a;
	bitops_setl(&aligned_a, 0, 2, 0);
	l = mem_readl(cpu, aligned_a);

	/* Update value */
	v = cpu->R[cpu->instruction.i_type.rt];
	switch (bitops_getl(&a, 0, 2)) {
	case 0:
		bitops_setl(&l, 0, 8, bitops_getl(&v, 24, 8));
		break;
	case 1:
		bitops_setl(&l, 0, 16, bitops_getl(&v, 16, 16));
		break;
	case 2:
		bitops_setl(&l, 0, 24, bitops_getl(&v, 8, 24));
		break;
	case 3:
		l = v;
		break;
	}

	/* Store updated value at 32-bit aligned address */
	mem_writel(cpu, l, aligned_a);
}

void SWR(struct r3051 *cpu)
{
	uint32_t a;
	uint32_t aligned_a;
	uint32_t l;
	uint32_t v;

	/* Read value at 32-bit aligned address */
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;
	aligned_a = a;
	bitops_setl(&aligned_a, 0, 2, 0);
	l = mem_readl(cpu, aligned_a);

	/* Update value */
	v = cpu->R[cpu->instruction.i_type.rt];
	switch (bitops_getl(&a, 0, 2)) {
	case 0:
		l = v;
		break;
	case 1:
		bitops_setl(&l, 8, 24, v);
		break;
	case 2:
		bitops_setl(&l, 16, 16, v);
		break;
	case 3:
		bitops_setl(&l, 24, 8, v);
		break;
	}

	/* Store updated value at 32-bit aligned address */
	mem_writel(cpu, l, aligned_a);
}

void ADDI(struct r3051 *cpu)
{
	uint32_t result;
	uint32_t v;
	int16_t immediate;
	bool overflow;

	/* Compute result and check for overflow */
	v = cpu->R[cpu->instruction.i_type.rs];
	immediate = (int16_t)cpu->instruction.i_type.immediate;
	result = v + immediate;
	overflow = ((~(v ^ immediate)) & (v ^ result)) & 0x80000000;

	/* Trap on overflow */
	if (overflow) {
		r3051_raise_exception(cpu, EXCEPTION_Ovf);
		return;
	}

	/* Update target register */
	r3051_set(cpu, cpu->instruction.i_type.rt, result);
}

void ADDIU(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.i_type.rs];
	l += (int16_t)cpu->instruction.i_type.immediate;
	r3051_set(cpu, cpu->instruction.i_type.rt, l);
}

void SLTI(struct r3051 *cpu)
{
	int32_t a = cpu->R[cpu->instruction.i_type.rs];
	int32_t b = (int16_t)cpu->instruction.i_type.immediate;
	uint32_t result = (a < b);
	r3051_set(cpu, cpu->instruction.i_type.rt, result);
}

void SLTIU(struct r3051 *cpu)
{
	uint32_t a = cpu->R[cpu->instruction.i_type.rs];
	uint32_t b = (int16_t)cpu->instruction.i_type.immediate;
	uint32_t result = (a < b);
	r3051_set(cpu, cpu->instruction.i_type.rt, result);
}

void ANDI(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.i_type.rs];
	l &= cpu->instruction.i_type.immediate;
	r3051_set(cpu, cpu->instruction.i_type.rt, l);
}

void ORI(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.i_type.rs];
	l |= cpu->instruction.i_type.immediate;
	r3051_set(cpu, cpu->instruction.i_type.rt, l);
}

void XORI(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.i_type.rs];
	l ^= cpu->instruction.i_type.immediate;
	r3051_set(cpu, cpu->instruction.i_type.rt, l);
}

void LUI(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->instruction.i_type.immediate << 16;
	r3051_set(cpu, cpu->instruction.i_type.rt, l);
}

void ADD(struct r3051 *cpu)
{
	uint32_t result;
	uint32_t v1;
	uint32_t v2;
	bool overflow;

	/* Compute result and check for overflow */
	v1 = cpu->R[cpu->instruction.r_type.rs];
	v2 = cpu->R[cpu->instruction.r_type.rt];
	result = v1 + v2;
	overflow = ((~(v1 ^ v2)) & (v1 ^ result)) & 0x80000000;

	/* Trap on overflow */
	if (overflow) {
		r3051_raise_exception(cpu, EXCEPTION_Ovf);
		return;
	}

	/* Update destination register */
	r3051_set(cpu, cpu->instruction.r_type.rd, result);
}

void ADDU(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.r_type.rs];
	l += cpu->R[cpu->instruction.r_type.rt];
	r3051_set(cpu, cpu->instruction.r_type.rd, l);
}

void SUB(struct r3051 *cpu)
{
	uint32_t result;
	uint32_t v1;
	uint32_t v2;
	bool overflow;

	/* Compute result and check for overflow */
	v1 = cpu->R[cpu->instruction.r_type.rs];
	v2 = cpu->R[cpu->instruction.r_type.rt];
	result = v1 - v2;
	overflow = (((v1 ^ v2)) & (v1 ^ result)) & 0x80000000;

	/* Trap on overflow */
	if (overflow) {
		r3051_raise_exception(cpu, EXCEPTION_Ovf);
		return;
	}

	/* Update destination register */
	r3051_set(cpu, cpu->instruction.r_type.rd, result);
}

void SUBU(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.r_type.rs];
	l -= cpu->R[cpu->instruction.r_type.rt];
	r3051_set(cpu, cpu->instruction.r_type.rd, l);
}

void SLT(struct r3051 *cpu)
{
	int32_t a = cpu->R[cpu->instruction.r_type.rs];
	int32_t b = cpu->R[cpu->instruction.r_type.rt];
	r3051_set(cpu, cpu->instruction.r_type.rd, (a < b));
}

void SLTU(struct r3051 *cpu)
{
	uint32_t a = cpu->R[cpu->instruction.r_type.rs];
	uint32_t b = cpu->R[cpu->instruction.r_type.rt];
	r3051_set(cpu, cpu->instruction.r_type.rd, (a < b));
}

void AND(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.r_type.rs];
	l &= cpu->R[cpu->instruction.r_type.rt];
	r3051_set(cpu, cpu->instruction.r_type.rd, l);
}

void OR(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.r_type.rs];
	l |= cpu->R[cpu->instruction.r_type.rt];
	r3051_set(cpu, cpu->instruction.r_type.rd, l);
}

void XOR(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.r_type.rs];
	l ^= cpu->R[cpu->instruction.r_type.rt];
	r3051_set(cpu, cpu->instruction.r_type.rd, l);
}

void NOR(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.r_type.rs];
	l |= cpu->R[cpu->instruction.r_type.rt];
	r3051_set(cpu, cpu->instruction.r_type.rd, ~l);
}

void SLL(struct r3051 *cpu)
{
	uint32_t v;
	v = cpu->R[cpu->instruction.r_type.rt];
	v <<= cpu->instruction.r_type.shamt;
	r3051_set(cpu, cpu->instruction.r_type.rd, v);
}

void SRL(struct r3051 *cpu)
{
	uint32_t v;
	v = cpu->R[cpu->instruction.r_type.rt];
	v >>= cpu->instruction.r_type.shamt;
	r3051_set(cpu, cpu->instruction.r_type.rd, v);
}

void SRA(struct r3051 *cpu)
{
	int32_t v;
	v = cpu->R[cpu->instruction.r_type.rt];
	v >>= cpu->instruction.r_type.shamt;
	r3051_set(cpu, cpu->instruction.r_type.rd, v);
}

void SLLV(struct r3051 *cpu)
{
	uint32_t v;
	uint32_t shift;
	v = cpu->R[cpu->instruction.r_type.rt];
	shift = bitops_getl(&cpu->R[cpu->instruction.r_type.rs], 0, 5);
	v <<= shift;
	r3051_set(cpu, cpu->instruction.r_type.rd, v);
}

void SRLV(struct r3051 *cpu)
{
	uint32_t v;
	uint32_t shift;
	v = cpu->R[cpu->instruction.r_type.rt];
	shift = bitops_getl(&cpu->R[cpu->instruction.r_type.rs], 0, 5);
	v >>= shift;
	r3051_set(cpu, cpu->instruction.r_type.rd, v);
}

void SRAV(struct r3051 *cpu)
{
	int32_t v;
	uint32_t shift;
	v = cpu->R[cpu->instruction.r_type.rt];
	shift = bitops_getl(&cpu->R[cpu->instruction.r_type.rs], 0, 5);
	v >>= shift;
	r3051_set(cpu, cpu->instruction.r_type.rd, v);
}

void MULT(struct r3051 *cpu)
{
	int64_t a = (int32_t)cpu->R[cpu->instruction.r_type.rs];
	int64_t b = (int32_t)cpu->R[cpu->instruction.r_type.rt];
	uint64_t result = a * b;
	cpu->HI = result >> 32;
	cpu->LO = result;
}

void MULTU(struct r3051 *cpu)
{
	uint64_t a = cpu->R[cpu->instruction.r_type.rs];
	uint64_t b = cpu->R[cpu->instruction.r_type.rt];
	uint64_t result = a * b;
	cpu->HI = result >> 32;
	cpu->LO = result;
}

void DIV(struct r3051 *cpu)
{
	int32_t n;
	int32_t d;
	n = cpu->R[cpu->instruction.r_type.rs];
	d = cpu->R[cpu->instruction.r_type.rt];
	if (d == 0) {
		cpu->HI = n;
		cpu->LO = (n >= 0) ? -1 : 1;
	} else if (((uint32_t)n == 0x80000000) && (d == -1)) {
		cpu->HI = 0;
		cpu->LO = n;
	} else {
		cpu->HI = n % d;
		cpu->LO = n / d;
	}
}

void DIVU(struct r3051 *cpu)
{
	uint32_t n;
	uint32_t d;
	n = cpu->R[cpu->instruction.r_type.rs];
	d = cpu->R[cpu->instruction.r_type.rt];
	cpu->LO = (d != 0) ? n / d : 0xFFFFFFFF;
	cpu->HI = (d != 0) ? n % d : n;
}

void MFHI(struct r3051 *cpu)
{
	r3051_set(cpu, cpu->instruction.r_type.rd, cpu->HI);
}

void MFLO(struct r3051 *cpu)
{
	r3051_set(cpu, cpu->instruction.r_type.rd, cpu->LO);
}

void MTHI(struct r3051 *cpu)
{
	cpu->HI = cpu->R[cpu->instruction.r_type.rs];
}

void MTLO(struct r3051 *cpu)
{
	cpu->LO = cpu->R[cpu->instruction.r_type.rs];
}

void J(struct r3051 *cpu)
{
	uint32_t PC;
	uint32_t a;
	PC = cpu->PC;
	a = cpu->instruction.j_type.target << 2;
	bitops_setl(&PC, 0, 28, a);
	r3051_branch(cpu, PC);
}

void JAL(struct r3051 *cpu)
{
	uint32_t PC;
	uint32_t a;
	r3051_set(cpu, 31, cpu->PC + 4);
	PC = cpu->PC;
	a = cpu->instruction.j_type.target << 2;
	bitops_setl(&PC, 0, 28, a);
	r3051_branch(cpu, PC);
}

void JR(struct r3051 *cpu)
{
	uint32_t address = cpu->R[cpu->instruction.r_type.rs];
	r3051_branch(cpu, address);
}

void JALR(struct r3051 *cpu)
{
	uint32_t address;
	address = cpu->R[cpu->instruction.r_type.rs];
	r3051_set(cpu, cpu->instruction.r_type.rd, cpu->PC + 4);
	r3051_branch(cpu, address);
}

void BEQ(struct r3051 *cpu)
{
	uint32_t s;
	uint32_t t;
	int32_t off;
	s = cpu->R[cpu->instruction.i_type.rs];
	t = cpu->R[cpu->instruction.i_type.rt];
	if (s == t) {
		off = (int16_t)cpu->instruction.i_type.immediate << 2;
		r3051_branch(cpu, cpu->PC + off);
	}
}

void BNE(struct r3051 *cpu)
{
	uint32_t s;
	uint32_t t;
	int32_t off;
	s = cpu->R[cpu->instruction.i_type.rs];
	t = cpu->R[cpu->instruction.i_type.rt];
	if (s != t) {
		off = (int16_t)cpu->instruction.i_type.immediate << 2;
		r3051_branch(cpu, cpu->PC + off);
	}
}

void BLEZ(struct r3051 *cpu)
{
	int32_t s;
	int32_t off;
	s = cpu->R[cpu->instruction.i_type.rs];
	if (s <= 0) {
		off = (int16_t)cpu->instruction.i_type.immediate << 2;
		r3051_branch(cpu, cpu->PC + off);
	}
}

void BGTZ(struct r3051 *cpu)
{
	int32_t s;
	int32_t off;
	s = cpu->R[cpu->instruction.i_type.rs];
	if (s > 0) {
		off = (int16_t)cpu->instruction.i_type.immediate << 2;
		r3051_branch(cpu, cpu->PC + off);
	}
}

void BLTZ(struct r3051 *cpu)
{
	int32_t s;
	int32_t off;
	s = cpu->R[cpu->instruction.i_type.rs];
	if (s < 0) {
		off = (int16_t)cpu->instruction.i_type.immediate << 2;
		r3051_branch(cpu, cpu->PC + off);
	}
}

void BGEZ(struct r3051 *cpu)
{
	int32_t s;
	int32_t off;
	s = cpu->R[cpu->instruction.i_type.rs];
	if (s >= 0) {
		off = (int16_t)cpu->instruction.i_type.immediate << 2;
		r3051_branch(cpu, cpu->PC + off);
	}
}

void BLTZAL(struct r3051 *cpu)
{
	int32_t s;
	int32_t off;
	s = cpu->R[cpu->instruction.i_type.rs];
	r3051_set(cpu, 31, cpu->PC + 4);
	if (s < 0) {
		off = (int16_t)cpu->instruction.i_type.immediate << 2;
		r3051_branch(cpu, cpu->PC + off);
	}
}

void BGEZAL(struct r3051 *cpu)
{
	int32_t s;
	int32_t off;
	s = cpu->R[cpu->instruction.i_type.rs];
	r3051_set(cpu, 31, cpu->PC + 4);
	if (s >= 0) {
		off = (int16_t)cpu->instruction.i_type.immediate << 2;
		r3051_branch(cpu, cpu->PC + off);
	}
}

void SYSCALL(struct r3051 *cpu)
{
	r3051_raise_exception(cpu, EXCEPTION_Sys);
}

void BREAK(struct r3051 *cpu)
{
	r3051_raise_exception(cpu, EXCEPTION_Bp);
}

void MFC0(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->cop0.R[cpu->instruction.r_type.rd];
	r3051_load(cpu, cpu->instruction.r_type.rt, l);
}

void MTC0(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.r_type.rt];
	cpu->cop0.R[cpu->instruction.r_type.rd] = l;
}

void RFE(struct r3051 *cpu)
{
	/* Shift interrupt enable/kernel-user mode bits back */
	cpu->cop0.stat.IEc = cpu->cop0.stat.IEp;
	cpu->cop0.stat.KUc = cpu->cop0.stat.KUp;
	cpu->cop0.stat.IEp = cpu->cop0.stat.IEo;
	cpu->cop0.stat.KUp = cpu->cop0.stat.KUo;
}

void r3051_fetch(struct r3051 *cpu)
{
	bool cache_access;
	union address address;
	struct cache_line *line;
	bool valid_instruction;
	uint32_t a;
	int i;

	/* Handle alignment exception */
	if (cpu->PC & 0x03)
		r3051_raise_exception(cpu, EXCEPTION_AdEL);

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

void r3051_handle_interrupts(struct r3051 *cpu)
{
	/* Return if no interrupt is pending */
	if (cpu->cop0.cause.IP == 0)
		return;

	/* Return if interrupts are globally disabled */
	if (!cpu->cop0.stat.IEc || !cpu->cop0.stat.Intr)
		return;

	/* Raise exception */
	r3051_raise_exception(cpu, EXCEPTION_Int);
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

void r3051_load(struct r3051 *cpu, uint8_t reg, uint32_t data)
{
	/* Discard pending load if same register is being loaded */
	if (cpu->load_delay.delay && (reg == cpu->load_delay.reg))
		cpu->load_delay.delay = false;

	/* Handle any pending load immediately */
	if (cpu->load_delay.delay) {
		cpu->R[cpu->load_delay.reg] = cpu->load_delay.data;
		cpu->load_delay.delay = false;
	}

	/* Set load delay register/data pair and pending flag */
	cpu->load_delay.reg = reg;
	cpu->load_delay.data = data;
	cpu->load_delay.pending = true;
}

void r3051_set(struct r3051 *cpu, uint8_t reg, uint32_t data)
{
	/* Discard pending load if same register is being set */
	if (cpu->load_delay.delay && (reg == cpu->load_delay.reg))
		cpu->load_delay.delay = false;

	/* Update requested register */
	cpu->R[reg] = data;
}

void r3051_tick(struct r3051 *cpu)
{
	/* Save current PC */
	cpu->current_PC = cpu->PC;

	/* Check for interrupt requests */
	r3051_handle_interrupts(cpu);

	/* Fetch instruction */
	r3051_fetch(cpu);

	/* Handle pending branch delay (branch was taken in previous cycle) */
	if (cpu->branch_delay.pending) {
		cpu->branch_delay.delay = true;
		cpu->branch_delay.pending = false;
	}

	/* Handle pending load delay (load was made in previous cycle) */
	if (cpu->load_delay.pending) {
		cpu->load_delay.delay = true;
		cpu->load_delay.pending = false;
	}

	/* Execute instruction */
	switch (cpu->instruction.opcode) {
	case 0x00:
		r3051_opcode_SPECIAL(cpu);
		break;
	case 0x01:
		r3051_opcode_BCOND(cpu);
		break;
	case 0x02:
		J(cpu);
		break;
	case 0x03:
		JAL(cpu);
		break;
	case 0x04:
		BEQ(cpu);
		break;
	case 0x05:
		BNE(cpu);
		break;
	case 0x06:
		BLEZ(cpu);
		break;
	case 0x07:
		BGTZ(cpu);
		break;
	case 0x08:
		ADDI(cpu);
		break;
	case 0x09:
		ADDIU(cpu);
		break;
	case 0x0A:
		SLTI(cpu);
		break;
	case 0x0B:
		SLTIU(cpu);
		break;
	case 0x0C:
		ANDI(cpu);
		break;
	case 0x0D:
		ORI(cpu);
		break;
	case 0x0E:
		XORI(cpu);
		break;
	case 0x0F:
		LUI(cpu);
		break;
	case 0x10:
		r3051_opcode_COP0(cpu);
		break;
	case 0x20:
		LB(cpu);
		break;
	case 0x21:
		LH(cpu);
		break;
	case 0x22:
		LWL(cpu);
		break;
	case 0x23:
		LW(cpu);
		break;
	case 0x24:
		LBU(cpu);
		break;
	case 0x25:
		LHU(cpu);
		break;
	case 0x26:
		LWR(cpu);
		break;
	case 0x28:
		SB(cpu);
		break;
	case 0x29:
		SH(cpu);
		break;
	case 0x2A:
		SWL(cpu);
		break;
	case 0x2B:
		SW(cpu);
		break;
	case 0x2E:
		SWR(cpu);
		break;
	default:
		LOG_W("Unknown opcode (%02x)!\n", cpu->instruction.opcode);
		break;
	}

	/* Handle branch delay (PC now needs to be updated) */
	if (cpu->branch_delay.delay) {
		cpu->PC = cpu->branch_delay.PC;
		cpu->branch_delay.delay = false;
	}

	/* Handle load delay (register now needs to be updated) */
	if (cpu->load_delay.delay) {
		cpu->R[cpu->load_delay.reg] = cpu->load_delay.data;
		cpu->load_delay.delay = false;
	}

	/* Always consume one cycle */
	clock_consume(1);
}

void r3051_opcode_SPECIAL(struct r3051 *cpu)
{
	/* Execute SPECIAL instruction */
	switch (cpu->instruction.special.opcode) {
	case 0x00:
		SLL(cpu);
		break;
	case 0x02:
		SRL(cpu);
		break;
	case 0x03:
		SRA(cpu);
		break;
	case 0x04:
		SLLV(cpu);
		break;
	case 0x06:
		SRLV(cpu);
		break;
	case 0x07:
		SRAV(cpu);
		break;
	case 0x08:
		JR(cpu);
		break;
	case 0x09:
		JALR(cpu);
		break;
	case 0x0C:
		SYSCALL(cpu);
		break;
	case 0x0D:
		BREAK(cpu);
		break;
	case 0x10:
		MFHI(cpu);
		break;
	case 0x11:
		MTHI(cpu);
		break;
	case 0x12:
		MFLO(cpu);
		break;
	case 0x13:
		MTLO(cpu);
		break;
	case 0x18:
		MULT(cpu);
		break;
	case 0x19:
		MULTU(cpu);
		break;
	case 0x1A:
		DIV(cpu);
		break;
	case 0x1B:
		DIVU(cpu);
		break;
	case 0x20:
		ADD(cpu);
		break;
	case 0x21:
		ADDU(cpu);
		break;
	case 0x22:
		SUB(cpu);
		break;
	case 0x23:
		SUBU(cpu);
		break;
	case 0x24:
		AND(cpu);
		break;
	case 0x25:
		OR(cpu);
		break;
	case 0x26:
		XOR(cpu);
		break;
	case 0x27:
		NOR(cpu);
		break;
	case 0x2A:
		SLT(cpu);
		break;
	case 0x2B:
		SLTU(cpu);
		break;
	default:
		LOG_W("Unknown SPECIAL opcode (%02x)!\n",
			cpu->instruction.special.opcode);
		break;
	}
}

void r3051_opcode_BCOND(struct r3051 *cpu)
{
	/* Execute BCOND instruction */
	switch (cpu->instruction.bcond.opcode) {
	case 0x00:
	case 0x02:
	case 0x04:
	case 0x06:
	case 0x08:
	case 0x0A:
	case 0x0C:
	case 0x0E:
	case 0x12:
	case 0x14:
	case 0x16:
	case 0x18:
	case 0x1A:
	case 0x1C:
	case 0x1E:
		BLTZ(cpu);
		break;
	case 0x01:
	case 0x03:
	case 0x05:
	case 0x07:
	case 0x09:
	case 0x0B:
	case 0x0D:
	case 0x0F:
	case 0x13:
	case 0x15:
	case 0x17:
	case 0x19:
	case 0x1B:
	case 0x1D:
	case 0x1F:
		BGEZ(cpu);
		break;
	case 0x10:
		BLTZAL(cpu);
		break;
	case 0x11:
		BGEZAL(cpu);
		break;
	default:
		LOG_W("Unknown BCOND opcode (%02x)!\n",
			cpu->instruction.bcond.opcode);
		break;
	}
}

void r3051_opcode_COP0(struct r3051 *cpu)
{
	/* Execute COP0 instruction */
	switch (cpu->instruction.cop.opcode) {
	case 0x00:
		MFC0(cpu);
		break;
	case 0x04:
		MTC0(cpu);
		break;
	case 0x10:
		RFE(cpu);
		break;
	default:
		LOG_W("Unknown COP0 opcode (%02x)!\n",
			cpu->instruction.cop.opcode);
		break;
	}
}

void r3051_raise_exception(struct r3051 *cpu, enum exception e)
{
	/* Save exception code and branch delay flag */
	cpu->cop0.cause.ExcCode = e;
	cpu->cop0.cause.BD = cpu->branch_delay.pending;

	/* Set exception PC (decrementing it if within a branch delay) */
	cpu->cop0.EPC = cpu->current_PC;
	if (cpu->branch_delay.pending)
		cpu->cop0.EPC -= 4;

	/* Shift interrupt enable/kernel-user mode bits */
	cpu->cop0.stat.IEo = cpu->cop0.stat.IEp;
	cpu->cop0.stat.KUo = cpu->cop0.stat.KUp;
	cpu->cop0.stat.IEp = cpu->cop0.stat.IEc;
	cpu->cop0.stat.KUp = cpu->cop0.stat.KUc;
	cpu->cop0.stat.IEc = 0;
	cpu->cop0.stat.KUc = 0;

	/* Update PC based on BEV flag */
	cpu->PC = cpu->cop0.stat.BEV ? EXCEPTION_ADDR_1 : EXCEPTION_ADDR_0;

	/* Reset any pending branch delay */
	cpu->branch_delay.delay = false;
	cpu->branch_delay.pending = false;
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

	/* Add interrupt control region */
	res = resource_get("int_control",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	cpu->int_ctrl_region.area = res;
	cpu->int_ctrl_region.mops = &int_ctrl_mops;
	cpu->int_ctrl_region.data = cpu;
	memory_region_add(&cpu->int_ctrl_region);

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
	cpu->i_stat = 0;
	cpu->i_mask = 0;
	cpu->branch_delay.delay = false;
	cpu->branch_delay.pending = false;
	cpu->load_delay.delay = false;
	cpu->load_delay.pending = false;

	/* Enable clock */
	cpu->clock.enabled = true;
}

void r3051_interrupt(struct cpu_instance *instance, int irq)
{
	struct r3051 *cpu = instance->priv_data;

	/* Update interrupt status register */
	cpu->i_stat |= (1 << irq);

	/* Update interrupt pending flag in cop0 cause register */
	cpu->cop0.cause.IP = ((cpu->i_stat & cpu->i_mask) != 0);
}

void r3051_deinit(struct cpu_instance *instance)
{
	free(instance->priv_data);
}

CPU_START(r3051)
	.init = r3051_init,
	.reset = r3051_reset,
	.interrupt = r3051_interrupt,
	.deinit = r3051_deinit
CPU_END

