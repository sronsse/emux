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
#define MAX_UNR_INDEX			0x100
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
	uint32_t raw;
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
static void r3051_opcode_COP2(struct r3051 *cpu);
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

static inline void LWC2(struct r3051 *cpu);
static inline void SWC2(struct r3051 *cpu);
static inline void MFC2(struct r3051 *cpu);
static inline void CFC2(struct r3051 *cpu);
static inline void MTC2(struct r3051 *cpu);
static inline void CTC2(struct r3051 *cpu);

static uint32_t cop2_read_dr(union cop2 *cop2, int index);
static void cop2_write_dr(union cop2 *cop2, int index, uint32_t v);
static uint32_t cop2_read_cr(union cop2 *cop2, int index);
static void cop2_write_cr(union cop2 *cop2, int index, uint32_t v);
static inline uint32_t cop2_divide(union cop2 *cop2);
static inline int64_t Ai(union cop2 *cop2, int i, int64_t value);
static inline int16_t Lm_Bi(union cop2 *cop2, int i, int32_t value, bool lm);
static inline int16_t Lm_Bi_PTZ(union cop2 *cop2, int, int32_t, int32_t, bool);
static inline uint8_t Lm_Ci(union cop2 *cop2, int i, int32_t value);
static inline int32_t Lm_D(union cop2 *cop2, int32_t value, bool chained);
static inline int64_t F(union cop2 *cop2, int64_t value);
static inline int32_t Lm_G(union cop2 *cop2, int i, int32_t value);
static inline int32_t Lm_H(union cop2 *cop2, int32_t value);
static inline void RTPS(struct r3051 *cpu);
static inline void NCLIP(struct r3051 *cpu);
static inline void OP(struct r3051 *cpu);
static inline void DPCS(struct r3051 *cpu);
static inline void INTPL(struct r3051 *cpu);
static inline void MVMVA(struct r3051 *cpu);
static inline void NCDS(struct r3051 *cpu);
static inline void CDP(struct r3051 *cpu);
static inline void NCDT(struct r3051 *cpu);
static inline void NCCS(struct r3051 *cpu);
static inline void CC(struct r3051 *cpu);
static inline void NCS(struct r3051 *cpu);
static inline void NCT(struct r3051 *cpu);
static inline void SQR(struct r3051 *cpu);
static inline void DCPL(struct r3051 *cpu);
static inline void DPCT(struct r3051 *cpu);
static inline void AVSZ3(struct r3051 *cpu);
static inline void AVSZ4(struct r3051 *cpu);
static inline void RTPT(struct r3051 *cpu);
static inline void GPF(struct r3051 *cpu);
static inline void GPL(struct r3051 *cpu);
static inline void NCCT(struct r3051 *cpu);

static struct mops int_ctrl_mops = {
	.readw = (readw_t)r3051_int_ctrl_readw,
	.readl = (readl_t)r3051_int_ctrl_readl,
	.writew = (writew_t)r3051_int_ctrl_writew,
	.writel = (writel_t)r3051_int_ctrl_writel
};

static uint8_t unr_table[MAX_UNR_INDEX + 1];

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

void MFC2(struct r3051 *cpu)
{
	uint32_t l;
	l = cop2_read_dr(&cpu->cop2, cpu->instruction.r_type.rd);
	r3051_load(cpu, cpu->instruction.r_type.rt, l);
}

void CFC2(struct r3051 *cpu)
{
	uint32_t l;
	l = cop2_read_cr(&cpu->cop2, cpu->instruction.r_type.rd);
	r3051_load(cpu, cpu->instruction.r_type.rt, l);
}

void MTC2(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.r_type.rt];
	cop2_write_dr(&cpu->cop2, cpu->instruction.r_type.rd, l);
}

void CTC2(struct r3051 *cpu)
{
	uint32_t l;
	l = cpu->R[cpu->instruction.r_type.rt];
	cop2_write_cr(&cpu->cop2, cpu->instruction.r_type.rd, l);
}

uint32_t cop2_read_dr(union cop2 *cop2, int index)
{
	uint32_t v;
	int16_t r;
	int16_t g;
	int16_t b;

	/* Handle COP2 data register read */
	switch (index) {
	case 1:
	case 3:
	case 5:
		/* Adapt index from 1/3/5 to 0/1/2 */
		index = (index - 1) / 2;

		/* Reading VZx returns a sign-extended 16-bit value */
		v = cop2->V[index].Z;
		break;
	case 8:
	case 9:
	case 10:
	case 11:
		/* Adapt index from 8/9/10/11 to 0/1/2/3 */
		index -= 8;

		/* Reading IRx returns a sign-extended 16-bit value */
		v = cop2->IR[index].data;
		break;
	case 28:
	case 29:
		/* Collapses 16:16:16 bit RGB (range 0000h..0F80h) to 5:5:5 bit
		RGB (range 0..1Fh). Negative values (8000h..FFFFh/80h) are
		saturated to 00h, large positive values (1000h..7FFFh/80h) are
		saturated to 1Fh. Any changes to IR1, IR2, IR3 are reflected to
		the ORGB and IRGB registers, so handle both cases at once.
		0-4   Red   (0..1Fh) IR1 divided by 80h, saturated to +00h..+1Fh
		5-9   Green (0..1Fh) IR2 divided by 80h, saturated to +00h..+1Fh
		10-14 Blue  (0..1Fh) IR3 divided by 80h, saturated to +00h..+1Fh
		15-31 Not used (always zero) */
		r = cop2->IR[1].data / 0x80;
		g = cop2->IR[2].data / 0x80;
		b = cop2->IR[3].data / 0x80;
		r = (r < 0) ? 0 : ((r > 0x1F) ? 0x1F : r);
		g = (g < 0) ? 0 : ((g > 0x1F) ? 0x1F : g);
		b = (b < 0) ? 0 : ((b > 0x1F) ? 0x1F : b);
		v = 0;
		bitops_setl(&v, 0, 5, r);
		bitops_setl(&v, 5, 5, g);
		bitops_setl(&v, 10, 5, b);
		break;
	default:
		/* Read data register as regular 32-bit */
		v = cop2->DR[index];
		break;
	}

	/* Return value */
	return v;
}

void cop2_write_dr(union cop2 *cop2, int index, uint32_t v)
{
	bool msb_a;
	bool msb_b;

	/* Handle COP2 data register write */
	switch (index) {
	case 7:
		/* OTZ register is 16-bit (so discard higher bits) */
		cop2->OTZ = v;
		break;
	case 29:
	case 31:
		/* ORGB and LZCR are read-only */
		break;
	case 16:
	case 17:
	case 18:
	case 19:
		/* Adapt index from 16/17/18/19 to 0/1/2/3 */
		index -= 16;

		/* SZ[0..3] are 16-bit (so discard higher bits) */
		cop2->SZ[index].Z = v;
		break;
	case 14:
		/* Write SXY2 register and SXYP (its mirror) */
		cop2->SXY[2].raw = v;
		cop2->SXY[3].raw = v;
		break;
	case 15:
		/* Write SXYP register */
		cop2->SXY[3].raw = v;

		/* Writing to SXYP moves SXY2/SXY1 to SXY1/SXY0. */
		cop2->SXY[0] = cop2->SXY[1];
		cop2->SXY[1] = cop2->SXY[2];
		cop2->SXY[2] = cop2->SXY[3];
		break;
	case 28:
		/* Expands 5:5:5 bit RGB (range 0..1Fh) to 16:16:16 bit RGB
		(range 0000h..0F80h).
		0-4   Red   (0..1Fh) multiplied by 80h, written to IR1
		5-9   Green (0..1Fh) multiplied by 80h, written to IR2
		10-14 Blue  (0..1Fh) multiplied by 80h, written to IR3
		15-31 Not used (always zero) */
		cop2->IR[1].data = bitops_getl(&v, 0, 5) * 0x80;
		cop2->IR[2].data = bitops_getl(&v, 5, 5) * 0x80;
		cop2->IR[3].data = bitops_getl(&v, 10, 5) * 0x80;
		break;
	case 30:
		/* Write LZCS register */
		cop2->LZCS = v;

		/* Update LZCR accordingly: reading LZCR returns the leading 0
		count of LZCS if LZCS is positive and the leading 1 count of
		LZCS if LZCS is negative. The results are in range 1..32. */
		cop2->LZCR = 0;
		msb_a = (bitops_getl(&v, 31, 1) != 0);
		do {
			cop2->LZCR++;
			v <<= 1;
			msb_b = (bitops_getl(&v, 31, 1) != 0);
		} while ((msb_a == msb_b) && (cop2->LZCR < 32));
		break;
	default:
		/* Write data register as regular 32-bit */
		cop2->DR[index] = v;
		break;
	}
}

uint32_t cop2_read_cr(union cop2 *cop2, int index)
{
	uint32_t v;

	/* Handle COP2 control register read */
	switch (index) {
	case 4:
		/* Reading RT33 returns a sign-extended 16-bit value */
		v = cop2->RT._33;
		break;
	case 12:
		/* Reading L33 returns a sign-extended 16-bit value */
		v = cop2->LLM._33;
		break;
	case 20:
		/* Reading LB3 returns a sign-extended 16-bit value */
		v = cop2->LCM._33;
		break;
	case 26:
		/* When reading the H register, the hardware does accidently
		sign-expand the unsigned 16-bit value (ie. values +8000h..+FFFFh
		are returned as FFFF8000h..FFFFFFFFh). */
		v = (int16_t)cop2->H;
		break;
	case 27:
		/* Reading DQA returns a sign-extended 16-bit value */
		v = cop2->DQA;
		break;
	case 29:
		/* Reading ZSF3 returns a sign-extended 16-bit value */
		v = cop2->ZSF3;
		break;
	case 30:
		/* Reading ZSF4 returns a sign-extended 16-bit value */
		v = cop2->ZSF4;
		break;
	default:
		/* Read control register as regular 32-bit */
		v = cop2->CR[index];
		break;
	}

	/* Return value */
	return v;
}

void cop2_write_cr(union cop2 *cop2, int index, uint32_t v)
{
	union cop2_flag flag;

	/* Handle COP2 control register write */
	switch (index) {
	case 4:
		/* RT33 register is 16-bit (so discard higher bits) */
		cop2->RT._33 = v;
		break;
	case 12:
		/* L33 register is 16-bit (so discard higher bits) */
		cop2->LLM._33 = v;
		break;
	case 20:
		/* LB3 register is 16-bit (so discard higher bits) */
		cop2->LCM._33 = v;
		break;
	case 26:
		/* H register is 16-bit (so discard higher bits) */
		cop2->H = v;
		break;
	case 27:
		/* DQA register is 16-bit (so discard higher bits) */
		cop2->DQA = v;
		break;
	case 29:
		/* ZSF3 register is 16-bit (so discard higher bits) */
		cop2->ZSF3 = v;
		break;
	case 30:
		/* ZSF4 register is 16-bit (so discard higher bits) */
		cop2->ZSF4 = v;
		break;
	case 31:
		/* Build flag taking care of read-only bits */
		flag.raw = v;
		flag.unused = 0;
		flag.error = 0;
		cop2->FLAG = flag;

		/* Update error flag (bits 30..23, and 18..13 or'ed together) */
		cop2->FLAG.error =
			cop2->FLAG.mac1_larger_pos |
			cop2->FLAG.mac2_larger_pos |
			cop2->FLAG.mac3_larger_pos |
			cop2->FLAG.mac1_larger_neg |
			cop2->FLAG.mac2_larger_neg |
			cop2->FLAG.mac3_larger_neg |
			cop2->FLAG.ir1_sat |
			cop2->FLAG.ir2_sat |
			cop2->FLAG.sz3_otz_sat |
			cop2->FLAG.div_overflow |
			cop2->FLAG.mac0_larger_pos |
			cop2->FLAG.mac0_larger_neg |
			cop2->FLAG.sx2_sat |
			cop2->FLAG.sy2_sat;
		break;
	default:
		/* Write control register as regular 32-bit */
		cop2->CR[index] = v;
		break;
	}
}

uint32_t cop2_divide(union cop2 *cop2)
{
	uint32_t n;
	uint32_t d;
	uint16_t divisor;
	int32_t u;
	int32_t t;
	int bit;
	int z;

	/* GTE Division Inaccuracy (for RTPS/RTPT commands)
	The GTE division does (attempt to) work as so (using 33bit math):
	n = (((H * 20000h / SZ3) + 1) / 2)

	Below would give (almost) the same result (using 32bit math):
	n = ((H * 10000h + SZ3 / 2) / SZ3)

	In both cases, the result is saturated as such:
	if n > 1FFFFh or division_by_zero then n = 1FFFFh, FLAG.Bit17 = 1,
	FLAG.Bit31 = 1

	However, the real GTE hardware is using a fast, but less accurate
	division mechanism (based on Unsigned Newton-Raphson algorithm). */

	/* Check if overflow */
	if (cop2->H < cop2->SZ[3].Z * 2) {
		/* z = 0..0Fh (for 16bit SZ3) */
		bit = bitops_fls(cop2->SZ[3].Z);
		z = (bit != 0) ? 16 - bit : 0;

		/* n = 0..7FFF8000h */
		n = cop2->H << z;

		/* d = 8000h..FFFFh */
		d = cop2->SZ[3].Z << z;

		/* u = 200h..101h */
		divisor = d | 0x8000;
		u = unr_table[((divisor & 0x7FFF) + 0x40) >> 7] + 0x101;

		/* t = 10000h..0FF01h */
		t = (((int32_t)divisor * -u) + 0x80) >> 8;

		/* t = 20000h..10000h */
		t = ((u * (0x20000 + t)) + 0x80) >> 8;

		/* n = 0..1FFFFh */
		n = (((uint64_t)n * t) + 0x8000) >> 16;
		if (n > 0x1FFFF)
			n = 0x1FFFF;
	} else {
		/* n = 1FFFFh plus overflow flag */
		n = 0x1FFFF;
		cop2->FLAG.div_overflow = 1;
	}

	/* Return division result */
	return n;
}

int64_t Ai(union cop2 *cop2, int i, int64_t value)
{
	/* Handle result larger than 43 bits and positive or negative */
	if (value >= (1LL << 43))
		switch (i) {
		case 1:
			cop2->FLAG.mac1_larger_pos = 1;
			break;
		case 2:
			cop2->FLAG.mac2_larger_pos = 1;
			break;
		case 3:
		default:
			cop2->FLAG.mac3_larger_pos = 1;
			break;
		}
	else if (value < -(1LL << 43))
		switch (i) {
		case 1:
			cop2->FLAG.mac1_larger_neg = 1;
			break;
		case 2:
			cop2->FLAG.mac2_larger_neg = 1;
			break;
		case 3:
		default:
			cop2->FLAG.mac3_larger_neg = 1;
			break;
		}

	/* Return result clamped to 44 bits while keeping sign */
	value <<= 20;
	value >>= 20;
	return value;
}

int16_t Lm_Bi(union cop2 *cop2, int i, int32_t value, bool lm)
{
	bool sat = false;

	/* Handle value negative (lm = 1) or larger than 15 bits (lm = 0) */
	if (lm && (value < 0)) {
		value = 0;
		sat = true;
	} else if (value > 32767) {
		value = 32767;
		sat = true;
	} else if (value < -32768) {
		value = -32768;
		sat = true;
	}

	/* Set flags accordingly */
	if (sat)
		switch (i) {
		case 1:
			cop2->FLAG.ir1_sat = 1;
			break;
		case 2:
			cop2->FLAG.ir2_sat = 1;
			break;
		case 3:
		default:
			cop2->FLAG.ir3_sat = 1;
			break;
		}

	/* Return result clamped to 16 bits */
	return value;
}

int16_t Lm_Bi_PTZ(union cop2 *cop2, int i, int32_t v1, int32_t v2, bool lm)
{
	/* Handle value negative (lm = 1) or larger than 15 bits (lm = 0) */
	if (lm && (v1 < 0))
		v1 = 0;
	else if (v1 > 32767)
		v1 = 32767;
	else if (v1 < -32768)
		v1 = -32768;

	/* Handle flag update if needed */
	if ((v2 > 32767) || (v2 < -32768))
		switch (i) {
		case 1:
			cop2->FLAG.ir1_sat = 1;
			break;
		case 2:
			cop2->FLAG.ir2_sat = 1;
			break;
		case 3:
		default:
			cop2->FLAG.ir3_sat = 1;
			break;
		}

	/* Return result clamped to 16 bits */
	return v1;
}

uint8_t Lm_Ci(union cop2 *cop2, int i, int32_t value)
{
	bool sat = false;

	/* Handle value negative or larger than 8 bits */
	if (value < 0) {
		value = 0;
		sat = true;
	} else if (value > 0xFF) {
		value = 0xFF;
		sat = true;
	}

	/* Set flags accordingly */
	if (sat)
		switch (i) {
		case 1:
			cop2->FLAG.r_sat = 1;
			break;
		case 2:
			cop2->FLAG.g_sat = 1;
			break;
		case 3:
		default:
			cop2->FLAG.b_sat = 1;
			break;
		}

	/* Return result clamped to 8 bits */
	return value;
}

int32_t Lm_D(union cop2 *cop2, int32_t value, bool chained)
{
	bool sat = false;

	/* Handle MAC0 overflow case if requested */
	if (chained) {
		if (cop2->FLAG.mac0_larger_neg) {
			value = 0;
			sat = true;
		}
		if (cop2->FLAG.mac0_larger_pos) {
			value = 0xFFFF;
			sat = true;
		}
	}

	/* Handle value negative or larger than 16 bits */
	if (value < 0) {
		value = 0;
		sat = true;
	} else if (value > 65535) {
		value = 65535;
		sat = true;
	}

	/* Set flags accordingly */
	if (sat)
		cop2->FLAG.sz3_otz_sat = 1;

	/* Return result clamped to 16 bits */
	return value;
}

int64_t F(union cop2 *cop2, int64_t value)
{
	/* Handle result larger than 31 bits and negative */
	if (value > 2147483647LL)
		cop2->FLAG.mac0_larger_pos = 1;
	else if (value < -2147483648LL)
		cop2->FLAG.mac0_larger_neg = 1;

	/* Return untouched value */
	return value;
}

int32_t Lm_G(union cop2 *cop2, int i, int32_t value)
{
	bool sat = false;

	/* Handle value larger than 10 bits */
	if (value < -1024) {
		value = -1024;
		sat = true;
	} else if (value > 1023) {
		value = 1023;
		sat = true;
	}

	/* Set flags accordingly */
	if (sat)
		switch (i) {
		case 1:
			cop2->FLAG.sx2_sat = 1;
			break;
		case 2:
		default:
			cop2->FLAG.sy2_sat = 1;
			break;
		}

	/* Return result clamped to 10 bits */
	return value;
}

int32_t Lm_H(union cop2 *cop2, int32_t value)
{
	bool sat = false;

	/* Handle value negative or larger than 12 bits */
	if (value < 0) {
		value = 0;
		sat = true;
	} else if (value > 4096) {
		value = 4096;
		sat = true;
	}

	/* Set flags accordingly */
	if (sat)
		cop2->FLAG.ir0_sat = 1;

	/* Return result clamped to 0..4096 */
	return value;
}

/* RTPS - Perspective Transformation (single) */
void RTPS(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	int64_t last;
	int64_t div;
	int16_t a;
	int16_t b;
	bool lm;
	int sf;
	int i;
	int j;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[TRX + R11 * VX0 + R12 * VY0 + R13 * VZ0]
	   MAC2 = A2[TRY + R21 * VX0 + R22 * VY0 + R23 * VZ0]
	   MAC3 = A3[TRZ + R31 * VX0 + R32 * VY0 + R33 * VZ0] */
	for (i = 0; i < 3; i++) {
		res = (uint64_t)(int64_t)cop2->TR.data[i] << 12;
		for (j = 0; j < 3; j++) {
			a = cop2->RT.data[i][j];
			b = cop2->V[0].data[j];
			res = Ai(cop2, i + 1, res + a * b);
		}
		cop2->MAC[i + 1] = res >> sf;
	}

	/* Save last result (MAC3) */
	last = res;

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	cop2->IR[1].data = Lm_Bi(cop2, 1, cop2->MAC[1], lm);
	cop2->IR[2].data = Lm_Bi(cop2, 2, cop2->MAC[2], lm);
	cop2->IR[3].data = Lm_Bi_PTZ(cop2, 3, cop2->MAC[3], last >> 12, lm);

	/* SZ0 <- SZ1 <- SZ2 <- SZ3 */
	cop2->SZ[0].Z = cop2->SZ[1].Z;
	cop2->SZ[1].Z = cop2->SZ[2].Z;
	cop2->SZ[2].Z = cop2->SZ[3].Z;

	/* SZ3 = Lm_D(MAC3) */
	cop2->SZ[3].Z = Lm_D(cop2, last >> 12, false);

	/* div = ((H * 20000h / SZ3) + 1) / 2 */
	div = cop2_divide(cop2);

	/* SX2 = Lm_G1[F[OFX + IR1 * (H / SZ)]] */
	res = F(cop2, cop2->OFX + cop2->IR[1].data * div) >> 16;
	cop2->MAC[0] = res;
	cop2->SXY[3].X = Lm_G(cop2, 1, cop2->MAC[0]);

	/* SY2 = Lm_G2[F[OFY + IR2 * (H / SZ)]] */
	res = F(cop2, cop2->OFY + cop2->IR[2].data * div) >> 16;
	cop2->MAC[0] = res;
	cop2->SXY[3].Y = Lm_G(cop2, 2, cop2->MAC[0]);

	/* SX0 <- SX1 <- SX2, SY0 <- SY1 <- SY2 */
	cop2->SXY[0] = cop2->SXY[1];
	cop2->SXY[1] = cop2->SXY[2];
	cop2->SXY[2] = cop2->SXY[3];

	/* MAC0 = F[DQB + DQA * (H / SZ)]
	   IR0 = Lm_H[MAC0] */
	res = cop2->DQB + cop2->DQA * div;
	cop2->MAC[0] = F(cop2, res);
	cop2->IR[0].data = Lm_H(cop2, res >> 12);

	clock_consume(23);
}

/* NCLIP - Normal clipping */
void NCLIP(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;

	/* MAC0 = F[SX0 * SY1 +
	            SX1 * SY2 +
	            SX2 * SY0 -
	            SX0 * SY2 -
	            SX1 * SY0 -
	            SX2 * SY1] */
	res = cop2->SXY[0].X * (cop2->SXY[1].Y - cop2->SXY[2].Y);
	res += cop2->SXY[1].X * (cop2->SXY[2].Y - cop2->SXY[0].Y);
	res += cop2->SXY[2].X * (cop2->SXY[0].Y - cop2->SXY[1].Y);
	cop2->MAC[0] = F(cop2, res);

	clock_consume(8);
}

/* OP - Outer product of 2 vectors */
void OP(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	bool lm;
	int sf;
	int i;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[D2 * IR3 - D3 * IR2] */
	res = cop2->RT.data[1][1] * cop2->IR[3].data;
	res -= cop2->RT.data[2][2] * cop2->IR[2].data;
	cop2->MAC[1] = Ai(cop2, 1, res >> sf);

	/* MAC2 = A2[D3 * IR1 - D1 * IR3] */
	res = cop2->RT.data[2][2] * cop2->IR[1].data;
	res -= cop2->RT.data[0][0] * cop2->IR[3].data;
	cop2->MAC[2] = Ai(cop2, 2, res >> sf);

	/* MAC3 = A3[D1 * IR2 - D2 * IR1] */
	res = cop2->RT.data[0][0] * cop2->IR[2].data;
	res -= cop2->RT.data[1][1] * cop2->IR[1].data;
	cop2->MAC[3] = Ai(cop2, 3, res >> sf);

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	clock_consume(6);
}

/* DPCS - Depth Cueing (single) */
void DPCS(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	uint8_t t;
	bool lm;
	int sf;
	int i;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[R + IR0 * (Lm_B1[RFC - R])]
	   MAC2 = A2[G + IR0 * (Lm_B2[GFC - G])]
	   MAC3 = A3[B + IR0 * (Lm_B3[BFC - B])] */
	for (i = 0; i < 3; i++) {
		t = cop2->RGBC.data[i];
		res = (uint64_t)(int64_t)cop2->FC.data[i] << 12;
		res -= (int32_t)((uint32_t)t << 16);
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], false);
		res *= cop2->IR[0].data;
		res += (int64_t)((uint64_t)(int64_t)t << 16);
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* Cd0 <- Cd1 <- Cd2 <- CODE */
	cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
	cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
	cop2->RGB_FIFO[2].C = cop2->RGBC.C;

	/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
	   G0 <- G1 <- G2 <- Lm_C2[MAC2]
	   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
	for (i = 0; i < 3; i++) {
		cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
		cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
		res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
		cop2->RGB_FIFO[2].data[i] = res;
	}

	clock_consume(8);
}

/* INTPL - Interpolation of a vector and far color */
void INTPL(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	bool lm;
	int sf;
	int i;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[IR1 + IR0 * (Lm_B1[RFC - IR1])]
	   MAC2 = A2[IR2 + IR0 * (Lm_B2[GFC - IR2])]
	   MAC3 = A3[IR3 + IR0 * (Lm_B3[BFC - IR3])] */
	for (i = 0; i < 3; i++) {
		res = (uint64_t)(int64_t)cop2->FC.data[i] << 12;
		res -= (int32_t)((uint32_t)(int32_t)cop2->IR[i + 1].data << 12);
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], false);
		res *= cop2->IR[0].data;
		res += (int64_t)((uint64_t)(int64_t)cop2->IR[i + 1].data << 12);
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res >> sf);
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* Cd0 <- Cd1 <- Cd2 <- CODE */
	cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
	cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
	cop2->RGB_FIFO[2].C = cop2->RGBC.C;

	/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
	   G0 <- G1 <- G2 <- Lm_C2[MAC2]
	   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
	for (i = 0; i < 3; i++) {
		cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
		cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
		res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
		cop2->RGB_FIFO[2].data[i] = res;
	}

	clock_consume(8);
}

/* MVMVA - Multiply vector by matrix and add vector */
void MVMVA(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	union cop2_matrix m;
	union cop2_vec16 v;
	union cop2_vec32 t;
	int64_t res;
	bool quirk;
	bool lm;
	int sf;
	int i;

	/* Set matrix */
	switch (cpu->instruction.cop2.mvmva_multiply_matrix) {
	case 0:
		m = cop2->RT;
		break;
	case 1:
		m = cop2->LLM;
		break;
	case 2:
		m = cop2->LCM;
		break;
	case 3:
		/* Set up garbage matrix */
		m.data[0][0] = -(cop2->RGBC.R << 4);
		m.data[0][1] = cop2->RGBC.R << 4;
		m.data[0][2] = cop2->IR[0].data;
		m.data[1][0] = (int16_t)cop2->CR[1];
		m.data[1][1] = (int16_t)cop2->CR[1];
		m.data[1][2] = (int16_t)cop2->CR[1];
		m.data[2][0] = (int16_t)cop2->CR[2];
		m.data[2][1] = (int16_t)cop2->CR[2];
		m.data[2][2] = (int16_t)cop2->CR[2];
		break;
	}

	/* Set vector */
	switch (cpu->instruction.cop2.mvmva_multiply_vector) {
	case 0:
		v = cop2->V[0];
		break;
	case 1:
		v = cop2->V[1];
		break;
	case 2:
		v = cop2->V[2];
		break;
	case 3:
		v.X = cop2->IR[1].data;
		v.Y = cop2->IR[2].data;
		v.Z = cop2->IR[3].data;
		break;
	}

	/* Set translation vector */
	quirk = false;
	switch (cpu->instruction.cop2.mvmva_translation_vector) {
	case 0:
		t = cop2->TR;
		break;
	case 1:
		t = cop2->BK;
		break;
	case 2:
		/* The GTE also allows selection of the far color vector (FC),
		but this vector is not added correctly by the hardware. */
		t = cop2->FC;
		quirk = true;
		break;
	case 3:
		t.X = 0;
		t.Y = 0;
		t.Z = 0;
		break;
	}

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[CV1 + MX11 * V1 + MX12 * V2 + MX13 * V3]
	   MAC2 = A2[CV2 + MX21 * V1 + MX22 * V2 + MX23 * V3]
	   MAC3 = A3[CV3 + MX31 * V1 + MX32 * V2 + MX33 * V3] */
	for (i = 0; i < 3; i++) {
		res = (uint64_t)(int64_t)t.data[i] << 12;
		res = Ai(cop2, i + 1, res + m.data[i][0] * v.data[0]);
		if (quirk) {
			Lm_Bi(cop2, i + 1, res >> sf, false);
			res = 0;
		}
		res = Ai(cop2, i + 1, res + m.data[i][1] * v.data[1]);
		res = Ai(cop2, i + 1, res + m.data[i][2] * v.data[2]);
		cop2->MAC[i + 1] = res >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	clock_consume(8);
}

/* NCDS - Normal color depth cue (single vector) */
void NCDS(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	int16_t a;
	int16_t b;
	bool lm;
	int sf;
	int i;
	int j;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[L11 * VX0 + L12 * VY0 + L13 * VZ0]
	   MAC2 = A2[L21 * VX0 + L22 * VY0 + L23 * VZ0]
	   MAC3 = A3[L31 * VX0 + L32 * VY0 + L33 * VZ0] */
	for (i = 0; i < 3; i++) {
		res = 0;
		for (j = 0; j < 3; j++) {
			a = cop2->LLM.data[i][j];
			b = cop2->V[0].data[j];
			res = Ai(cop2, i + 1, res + a * b);
		}
		cop2->MAC[i + 1] = res >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* MAC1 = A1[RBK + LR1 * IR1 + LR2 * IR2 + LR3 * IR3]
	   MAC2 = A2[GBK + LG1 * IR1 + LG2 * IR2 + LG3 * IR3]
	   MAC3 = A3[BBK + LB1 * IR1 + LB2 * IR2 + LB3 * IR3] */
	for (i = 0; i < 3; i++) {
		res = (uint64_t)(int64_t)cop2->BK.data[i] << 12;
		for (j = 0; j < 3; j++) {
			a = cop2->LCM.data[i][j];
			b = cop2->IR[j + 1].data;
			res = Ai(cop2, i + 1, res + a * b);
		}
		cop2->MAC[i + 1] = res >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* MAC1 = A1[R * IR1 + IR0 * (Lm_B1[RFC - R * IR1])]
	   MAC2 = A2[G * IR2 + IR0 * (Lm_B2[GFC - G * IR2])]
	   MAC3 = A3[B * IR3 + IR0 * (Lm_B3[BFC - B * IR3])] */
	for (i = 0; i < 3; i++) {
		res = (uint64_t)(int64_t)cop2->FC.data[i] << 12;
		res -= (cop2->RGBC.data[i] << 4) * cop2->IR[i + 1].data;
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], false);
		res *= cop2->IR[0].data;
		res += (cop2->RGBC.data[i] << 4) * cop2->IR[i + 1].data;
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* Cd0 <- Cd1 <- Cd2 <- CODE */
	cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
	cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
	cop2->RGB_FIFO[2].C = cop2->RGBC.C;

	/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
	   G0 <- G1 <- G2 <- Lm_C2[MAC2]
	   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
	for (i = 0; i < 3; i++) {
		cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
		cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
		res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
		cop2->RGB_FIFO[2].data[i] = res;
	}

	clock_consume(19);
}

/* CDP - Color Depth Cue */
void CDP(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	int16_t a;
	int16_t b;
	bool lm;
	int sf;
	int i;
	int j;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[RBK + LR1 * IR1 + LR2 * IR2 + LR3 * IR3]
	   MAC2 = A2[GBK + LG1 * IR1 + LG2 * IR2 + LG3 * IR3]
	   MAC3 = A3[BBK + LB1 * IR1 + LB2 * IR2 + LB3 * IR3] */
	for (i = 0; i < 3; i++) {
		res = (uint64_t)(int64_t)cop2->BK.data[i] << 12;
		for (j = 0; j < 3; j++) {
			a = cop2->LCM.data[i][j];
			b = cop2->IR[j + 1].data;
			res = Ai(cop2, i + 1, res + a * b);
		}
		cop2->MAC[i + 1] = res >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* MAC1 = A1[R * IR1 + IR0 * (Lm_B1[RFC - R * IR1])]
	   MAC2 = A2[G * IR2 + IR0 * (Lm_B2[GFC - G * IR2])]
	   MAC3 = A3[B * IR3 + IR0 * (Lm_B3[BFC - B * IR3])] */
	for (i = 0; i < 3; i++) {
		res = (uint64_t)(int64_t)cop2->FC.data[i] << 12;
		res -= (cop2->RGBC.data[i] << 4) * cop2->IR[i + 1].data;
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], false);
		res *= cop2->IR[0].data;
		res += (cop2->RGBC.data[i] << 4) * cop2->IR[i + 1].data;
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* Cd0 <- Cd1 <- Cd2 <- CODE */
	cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
	cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
	cop2->RGB_FIFO[2].C = cop2->RGBC.C;

	/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
	   G0 <- G1 <- G2 <- Lm_C2[MAC2]
	   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
	for (i = 0; i < 3; i++) {
		cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
		cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
		res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
		cop2->RGB_FIFO[2].data[i] = res;
	}

	clock_consume(13);
}

/* NCDT - Normal color depth cue (triple vector) */
void NCDT(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	int16_t a;
	int16_t b;
	bool lm;
	int sf;
	int v;
	int i;
	int j;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* Handle 3 vectors */
	for (v = 0; v < 3; v++) {
		/* MAC1 = A1[L11 * VXv + L12 * VYv + L13 * VZv]
		   MAC2 = A2[L21 * VXv + L22 * VYv + L23 * VZv]
		   MAC3 = A3[L31 * VXv + L32 * VYv + L33 * VZv] */
		for (i = 0; i < 3; i++) {
			res = 0;
			for (j = 0; j < 3; j++) {
				a = cop2->LLM.data[i][j];
				b = cop2->V[v].data[j];
				res = Ai(cop2, i + 1, res + a * b);
			}
			cop2->MAC[i + 1] = res >> sf;
		}

		/* IR1 = Lm_B1[MAC1]
		   IR2 = Lm_B2[MAC2]
		   IR3 = Lm_B3[MAC3] */
		for (i = 0; i < 3; i++) {
			res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
			cop2->IR[i + 1].data = res;
		}

		/* MAC1 = A1[RBK + LR1 * IR1 + LR2 * IR2 + LR3 * IR3]
		   MAC2 = A2[GBK + LG1 * IR1 + LG2 * IR2 + LG3 * IR3]
		   MAC3 = A3[BBK + LB1 * IR1 + LB2 * IR2 + LB3 * IR3] */
		for (i = 0; i < 3; i++) {
			res = (uint64_t)(int64_t)cop2->BK.data[i] << 12;
			for (j = 0; j < 3; j++) {
				a = cop2->LCM.data[i][j];
				b = cop2->IR[j + 1].data;
				res = Ai(cop2, i + 1, res + a * b);
			}
			cop2->MAC[i + 1] = res >> sf;
		}

		/* IR1 = Lm_B1[MAC1]
		   IR2 = Lm_B2[MAC2]
		   IR3 = Lm_B3[MAC3] */
		for (i = 0; i < 3; i++) {
			res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
			cop2->IR[i + 1].data = res;
		}

		/* MAC1 = A1[R * IR1 + IR0 * (Lm_B1[RFC - R * IR1])]
		   MAC2 = A2[G * IR2 + IR0 * (Lm_B2[GFC - G * IR2])]
		   MAC3 = A3[B * IR3 + IR0 * (Lm_B3[BFC - B * IR3])] */
		for (i = 0; i < 3; i++) {
			res = (uint64_t)(int64_t)cop2->FC.data[i] << 12;
			res -= (cop2->RGBC.data[i] << 4) * cop2->IR[i + 1].data;
			cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
			res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], false);
			res *= cop2->IR[0].data;
			res += (cop2->RGBC.data[i] << 4) * cop2->IR[i + 1].data;
			cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
		}

		/* IR1 = Lm_B1[MAC1]
		   IR2 = Lm_B2[MAC2]
		   IR3 = Lm_B3[MAC3] */
		for (i = 0; i < 3; i++) {
			res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
			cop2->IR[i + 1].data = res;
		}

		/* Cd0 <- Cd1 <- Cd2 <- CODE */
		cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
		cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
		cop2->RGB_FIFO[2].C = cop2->RGBC.C;

		/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
		   G0 <- G1 <- G2 <- Lm_C2[MAC2]
		   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
		for (i = 0; i < 3; i++) {
			cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
			cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
			res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
			cop2->RGB_FIFO[2].data[i] = res;
		}
	}

	clock_consume(44);
}

/* NCCS - Normal Color Color (single vector) */
void NCCS(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	int16_t a;
	int16_t b;
	bool lm;
	int sf;
	int i;
	int j;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[L11 * VX0 + L12 * VY0 + L13 * VZ0]
	   MAC2 = A2[L21 * VX0 + L22 * VY0 + L23 * VZ0]
	   MAC3 = A3[L31 * VX0 + L32 * VY0 + L33 * VZ0] */
	for (i = 0; i < 3; i++) {
		res = 0;
		for (j = 0; j < 3; j++) {
			a = cop2->LLM.data[i][j];
			b = cop2->V[0].data[j];
			res = Ai(cop2, i + 1, res + a * b);
		}
		cop2->MAC[i + 1] = res >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* MAC1 = A1[RBK + LR1 * IR1 + LR2 * IR2 + LR3 * IR3]
	   MAC2 = A2[GBK + LG1 * IR1 + LG2 * IR2 + LG3 * IR3]
	   MAC3 = A3[BBK + LB1 * IR1 + LB2 * IR2 + LB3 * IR3] */
	for (i = 0; i < 3; i++) {
		res = (uint64_t)(int64_t)cop2->BK.data[i] << 12;
		for (j = 0; j < 3; j++) {
			a = cop2->LCM.data[i][j];
			b = cop2->IR[j + 1].data;
			res = Ai(cop2, i + 1, res + a * b);
		}
		cop2->MAC[i + 1] = res >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* MAC1 = A1[R * IR1]
	   MAC2 = A2[G * IR2]
	   MAC3 = A3[B * IR3] */
	cop2->MAC[1] = ((cop2->RGBC.R << 4) * cop2->IR[1].data) >> sf;
	cop2->MAC[2] = ((cop2->RGBC.G << 4) * cop2->IR[2].data) >> sf;
	cop2->MAC[3] = ((cop2->RGBC.B << 4) * cop2->IR[3].data) >> sf;

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* Cd0 <- Cd1 <- Cd2 <- CODE */
	cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
	cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
	cop2->RGB_FIFO[2].C = cop2->RGBC.C;

	/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
	   G0 <- G1 <- G2 <- Lm_C2[MAC2]
	   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
	for (i = 0; i < 3; i++) {
		cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
		cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
		res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
		cop2->RGB_FIFO[2].data[i] = res;
	}

	clock_consume(17);
}

/* CC - Color Color */
void CC(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	int16_t a;
	int16_t b;
	bool lm;
	int sf;
	int i;
	int j;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[RBK + LR1 * IR1 + LR2 * IR2 + LR3 * IR3]
	   MAC2 = A2[GBK + LG1 * IR1 + LG2 * IR2 + LG3 * IR3]
	   MAC3 = A3[BBK + LB1 * IR1 + LB2 * IR2 + LB3 * IR3] */
	for (i = 0; i < 3; i++) {
		res = (uint64_t)(int64_t)cop2->BK.data[i] << 12;
		for (j = 0; j < 3; j++) {
			a = cop2->LCM.data[i][j];
			b = cop2->IR[j + 1].data;
			res = Ai(cop2, i + 1, res + a * b);
		}
		cop2->MAC[i + 1] = res >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* MAC1 = A1[R * IR1]
	   MAC2 = A2[G * IR2]
	   MAC3 = A3[B * IR3] */
	cop2->MAC[1] = ((cop2->RGBC.R << 4) * cop2->IR[1].data) >> sf;
	cop2->MAC[2] = ((cop2->RGBC.G << 4) * cop2->IR[2].data) >> sf;
	cop2->MAC[3] = ((cop2->RGBC.B << 4) * cop2->IR[3].data) >> sf;

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* Cd0 <- Cd1 <- Cd2 <- CODE */
	cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
	cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
	cop2->RGB_FIFO[2].C = cop2->RGBC.C;

	/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
	   G0 <- G1 <- G2 <- Lm_C2[MAC2]
	   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
	for (i = 0; i < 3; i++) {
		cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
		cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
		res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
		cop2->RGB_FIFO[2].data[i] = res;
	}

	clock_consume(11);
}

/* NCS - Normal color (single) */
void NCS(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	int16_t a;
	int16_t b;
	bool lm;
	int sf;
	int i;
	int j;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[L11 * VX0 + L12 * VY0 + L13 * VZ0]
	   MAC2 = A2[L21 * VX0 + L22 * VY0 + L23 * VZ0]
	   MAC3 = A3[L31 * VX0 + L32 * VY0 + L33 * VZ0] */
	for (i = 0; i < 3; i++) {
		res = 0;
		for (j = 0; j < 3; j++) {
			a = cop2->LLM.data[i][j];
			b = cop2->V[0].data[j];
			res = Ai(cop2, i + 1, res + a * b);
		}
		cop2->MAC[i + 1] = res >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* MAC1 = A1[RBK + LR1 * IR1 + LR2 * IR2 + LR3 * IR3]
	   MAC2 = A2[GBK + LG1 * IR1 + LG2 * IR2 + LG3 * IR3]
	   MAC3 = A3[BBK + LB1 * IR1 + LB2 * IR2 + LB3 * IR3] */
	for (i = 0; i < 3; i++) {
		res = (uint64_t)(int64_t)cop2->BK.data[i] << 12;
		for (j = 0; j < 3; j++) {
			a = cop2->LCM.data[i][j];
			b = cop2->IR[j + 1].data;
			res = Ai(cop2, i + 1, res + a * b);
		}
		cop2->MAC[i + 1] = res >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* Cd0 <- Cd1 <- Cd2 <- CODE */
	cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
	cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
	cop2->RGB_FIFO[2].C = cop2->RGBC.C;

	/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
	   G0 <- G1 <- G2 <- Lm_C2[MAC2]
	   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
	for (i = 0; i < 3; i++) {
		cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
		cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
		res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
		cop2->RGB_FIFO[2].data[i] = res;
	}

	clock_consume(14);
}

/* NCT - Normal color (triple) */
void NCT(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	int16_t a;
	int16_t b;
	bool lm;
	int sf;
	int v;
	int i;
	int j;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* Handle 3 vectors */
	for (v = 0; v < 3; v++) {
		/* MAC1 = A1[L11 * VXv + L12 * VYv + L13 * VZv]
		   MAC2 = A2[L21 * VXv + L22 * VYv + L23 * VZv]
		   MAC3 = A3[L31 * VXv + L32 * VYv + L33 * VZv] */
		for (i = 0; i < 3; i++) {
			res = 0;
			for (j = 0; j < 3; j++) {
				a = cop2->LLM.data[i][j];
				b = cop2->V[v].data[j];
				res = Ai(cop2, i + 1, res + a * b);
			}
			cop2->MAC[i + 1] = res >> sf;
		}

		/* IR1 = Lm_B1[MAC1]
		   IR2 = Lm_B2[MAC2]
		   IR3 = Lm_B3[MAC3] */
		for (i = 0; i < 3; i++) {
			res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
			cop2->IR[i + 1].data = res;
		}

		/* MAC1 = A1[RBK + LR1 * IR1 + LR2 * IR2 + LR3 * IR3]
		   MAC2 = A2[GBK + LG1 * IR1 + LG2 * IR2 + LG3 * IR3]
		   MAC3 = A3[BBK + LB1 * IR1 + LB2 * IR2 + LB3 * IR3] */
		for (i = 0; i < 3; i++) {
			res = (uint64_t)(int64_t)cop2->BK.data[i] << 12;
			for (j = 0; j < 3; j++) {
				a = cop2->LCM.data[i][j];
				b = cop2->IR[j + 1].data;
				res = Ai(cop2, i + 1, res + a * b);
			}
			cop2->MAC[i + 1] = res >> sf;
		}

		/* IR1 = Lm_B1[MAC1]
		   IR2 = Lm_B2[MAC2]
		   IR3 = Lm_B3[MAC3] */
		for (i = 0; i < 3; i++) {
			res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
			cop2->IR[i + 1].data = res;
		}

		/* Cd0 <- Cd1 <- Cd2 <- CODE */
		cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
		cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
		cop2->RGB_FIFO[2].C = cop2->RGBC.C;

		/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
		   G0 <- G1 <- G2 <- Lm_C2[MAC2]
		   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
		for (i = 0; i < 3; i++) {
			cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
			cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
			res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
			cop2->RGB_FIFO[2].data[i] = res;
		}
	}

	clock_consume(30);
}

/* SQR - Square vector */
void SQR(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	bool lm;
	int sf;
	int i;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[IR1 * IR1]
	   MAC2 = A2[IR2 * IR2]
	   MAC3 = A3[IR3 * IR3] */
	for (i = 0; i < 3; i++) {
		res = cop2->IR[i + 1].data * cop2->IR[i + 1].data;
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res >> sf);
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	clock_consume(5);
}

/* DCPL - Depth Cue Color light */
void DCPL(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	bool lm;
	int sf;
	int i;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[R * IR1 + IR0 * (Lm_B1[RFC - R * IR1])]
	   MAC2 = A2[G * IR2 + IR0 * (Lm_B2[GFC - G * IR2])]
	   MAC3 = A3[B * IR3 + IR0 * (Lm_B3[BFC - B * IR3])] */
	for (i = 0; i < 3; i++) {
		res = (uint64_t)(int64_t)cop2->FC.data[i] << 12;
		res -= (cop2->RGBC.data[i] << 4) * cop2->IR[i + 1].data;
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], false);
		res *= cop2->IR[0].data;
		res += (cop2->RGBC.data[i] << 4) * cop2->IR[i + 1].data;
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* Cd0 <- Cd1 <- Cd2 <- CODE */
	cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
	cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
	cop2->RGB_FIFO[2].C = cop2->RGBC.C;

	/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
	   G0 <- G1 <- G2 <- Lm_C2[MAC2]
	   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
	for (i = 0; i < 3; i++) {
		cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
		cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
		res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
		cop2->RGB_FIFO[2].data[i] = res;
	}

	clock_consume(8);
}

/* DPCT - Depth Cueing (triple) */
void DPCT(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	uint8_t t;
	bool lm;
	int sf;
	int c;
	int i;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* Perform this calculation 3 times, so all three RGB values have been
	replaced by the depth cued RGB values. */
	for (c = 0; c < 3; c++) {
		/* MAC1 = A1[R0 + IR0 * (Lm_B1[RFC - R0])]
		   MAC2 = A2[G0 + IR0 * (Lm_B2[GFC - G0])]
		   MAC3 = A3[B0 + IR0 * (Lm_B3[BFC - B0])] */
		for (i = 0; i < 3; i++) {
			t = cop2->RGB_FIFO[0].data[i];
			res = (uint64_t)(int64_t)cop2->FC.data[i] << 12;
			res -= (int32_t)((uint32_t)t << 16);
			cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
			res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], false);
			res *= cop2->IR[0].data;
			res += (int64_t)((uint64_t)(int64_t)t << 16);
			cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
		}

		/* IR1 = Lm_B1[MAC1]
		   IR2 = Lm_B2[MAC2]
		   IR3 = Lm_B3[MAC3] */
		for (i = 0; i < 3; i++) {
			res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
			cop2->IR[i + 1].data = res;
		}

		/* Cd0 <- Cd1 <- Cd2 <- CODE */
		cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
		cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
		cop2->RGB_FIFO[2].C = cop2->RGBC.C;

		/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
		   G0 <- G1 <- G2 <- Lm_C2[MAC2]
		   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
		for (i = 0; i < 3; i++) {
			cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
			cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
			res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
			cop2->RGB_FIFO[2].data[i] = res;
		}
	}

	clock_consume(17);
}

/* AVSZ3 - Average of three Z values (for Triangles) */
void AVSZ3(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;

	/* MAC0 = F[ZSF3 * SZ1 + ZSF3 * SZ2 + ZSF3 * SZ3] */
	res = cop2->SZ[1].Z;
	res += cop2->SZ[2].Z;
	res += cop2->SZ[3].Z;
	res *= cop2->ZSF3;
	cop2->MAC[0] = F(cop2, res);

	/* OTZ = Lm_D[MAC0] */
	cop2->OTZ = Lm_D(cop2, cop2->MAC[0] >> 12, true);

	clock_consume(5);
}

/* AVSZ4 - Average of four Z values (for Quads) */
void AVSZ4(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;

	/* MAC0 = F[ZSF4 * SZ0 + ZSF4 * SZ1 + ZSF4 * SZ2 + ZSF4 * SZ3] */
	res = cop2->SZ[0].Z;
	res += cop2->SZ[1].Z;
	res += cop2->SZ[2].Z;
	res += cop2->SZ[3].Z;
	res *= cop2->ZSF4;
	cop2->MAC[0] = F(cop2, res);

	/* OTZ = Lm_D[MAC0] */
	cop2->OTZ = Lm_D(cop2, cop2->MAC[0] >> 12, true);

	clock_consume(6);
}

/* RTPT - Perspective Transformation (triple) */
void RTPT(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	int64_t l;
	int64_t div;
	int16_t a;
	int16_t b;
	bool lm;
	int sf;
	int v;
	int i;
	int j;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* Handle 3 vectors */
	for (v = 0; v < 3; v++) {
		/* MAC1 = A1[TRX + R11 * VXv + R12 * VYv + R13 * VZv]
		   MAC2 = A2[TRY + R21 * VXv + R22 * VYv + R23 * VZv]
		   MAC3 = A3[TRZ + R31 * VXv + R32 * VYv + R33 * VZv] */
		for (i = 0; i < 3; i++) {
			res = (uint64_t)(int64_t)cop2->TR.data[i] << 12;
			for (j = 0; j < 3; j++) {
				a = cop2->RT.data[i][j];
				b = cop2->V[v].data[j];
				res = Ai(cop2, i + 1, res + a * b);
			}
			cop2->MAC[i + 1] = res >> sf;
		}

		/* Save last result (MAC3) */
		l = res;

		/* IR1 = Lm_B1[MAC1]
		   IR2 = Lm_B2[MAC2]
		   IR3 = Lm_B3[MAC3] */
		cop2->IR[1].data = Lm_Bi(cop2, 1, cop2->MAC[1], lm);
		cop2->IR[2].data = Lm_Bi(cop2, 2, cop2->MAC[2], lm);
		cop2->IR[3].data = Lm_Bi_PTZ(cop2, 3, cop2->MAC[3], l >> 12, lm);

		/* SZ0 <- SZ1 <- SZ2 <- SZ3 */
		cop2->SZ[0].Z = cop2->SZ[1].Z;
		cop2->SZ[1].Z = cop2->SZ[2].Z;
		cop2->SZ[2].Z = cop2->SZ[3].Z;

		/* SZ3 = Lm_D(MAC3) */
		cop2->SZ[3].Z = Lm_D(cop2, l >> 12, false);

		/* div = ((H * 20000h / SZ3) + 1) / 2 */
		div = cop2_divide(cop2);

		/* SX2 = Lm_G1[F[OFX + IR1 * (H / SZ)]] */
		cop2->MAC[0] = F(cop2, cop2->OFX + cop2->IR[1].data * div) >> 16;
		cop2->SXY[3].X = Lm_G(cop2, 1, cop2->MAC[0]);

		/* SY2 = Lm_G2[F[OFY + IR2 * (H / SZ)]] */
		cop2->MAC[0] = F(cop2, cop2->OFY + cop2->IR[2].data * div) >> 16;
		cop2->SXY[3].Y = Lm_G(cop2, 2, cop2->MAC[0]);

		/* SX0 <- SX1 <- SX2, SY0 <- SY1 <- SY2 */
		cop2->SXY[0] = cop2->SXY[1];
		cop2->SXY[1] = cop2->SXY[2];
		cop2->SXY[2] = cop2->SXY[3];
	}

	/* MAC0 = F[DQB + DQA * (H / SZ)]
	   IR0 = Lm_H[MAC0] */
	res = cop2->DQB + cop2->DQA * div;
	cop2->MAC[0] = F(cop2, res);
	cop2->IR[0].data = Lm_H(cop2, res >> 12);

	clock_consume(23);
}

/* GPF - General purpose interpolation */
void GPF(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	bool lm;
	int sf;
	int i;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[IR0 * IR1]
	   MAC2 = A2[IR0 * IR2]
	   MAC3 = A3[IR0 * IR3] */
	for (i = 0; i < 3; i++) {
		res = cop2->IR[0].data * cop2->IR[i + 1].data;
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* Cd0 <- Cd1 <- Cd2 <- CODE */
	cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
	cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
	cop2->RGB_FIFO[2].C = cop2->RGBC.C;

	/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
	   G0 <- G1 <- G2 <- Lm_C2[MAC2]
	   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
	for (i = 0; i < 3; i++) {
		cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
		cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
		res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
		cop2->RGB_FIFO[2].data[i] = res;
	}

	clock_consume(5);
}

/* GPL - General purpose interpolation with base */
void GPL(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	bool lm;
	int sf;
	int i;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* MAC1 = A1[MAC1 + IR0 * IR1]
	   MAC2 = A2[MAC2 + IR0 * IR2]
	   MAC3 = A3[MAC3 + IR0 * IR3] */
	for (i = 0; i < 3; i++) {
		res = (uint64_t)(int64_t)cop2->MAC[i + 1] << sf;
		res += cop2->IR[0].data * cop2->IR[i + 1].data;
		cop2->MAC[i + 1] = Ai(cop2, i + 1, res) >> sf;
	}

	/* IR1 = Lm_B1[MAC1]
	   IR2 = Lm_B2[MAC2]
	   IR3 = Lm_B3[MAC3] */
	for (i = 0; i < 3; i++) {
		res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
		cop2->IR[i + 1].data = res;
	}

	/* Cd0 <- Cd1 <- Cd2 <- CODE */
	cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
	cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
	cop2->RGB_FIFO[2].C = cop2->RGBC.C;

	/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
	   G0 <- G1 <- G2 <- Lm_C2[MAC2]
	   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
	for (i = 0; i < 3; i++) {
		cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
		cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
		res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
		cop2->RGB_FIFO[2].data[i] = res;
	}

	clock_consume(5);
}

/* NCCT - Normal Color Color (triple vector) */
void NCCT(struct r3051 *cpu)
{
	union cop2 *cop2 = &cpu->cop2;
	int64_t res;
	int16_t a;
	int16_t b;
	bool lm;
	int sf;
	int v;
	int i;
	int j;

	/* Get lm/sf bits from instruction */
	lm = cpu->instruction.cop2.lm;
	sf = cpu->instruction.cop2.sf ? 12 : 0;

	/* Handle 3 vectors */
	for (v = 0; v < 3; v++) {
		/* MAC1 = A1[L11 * VXv + L12 * VYv + L13 * VZv]
		   MAC2 = A2[L21 * VXv + L22 * VYv + L23 * VZv]
		   MAC3 = A3[L31 * VXv + L32 * VYv + L33 * VZv] */
		for (i = 0; i < 3; i++) {
			res = 0;
			for (j = 0; j < 3; j++) {
				a = cop2->LLM.data[i][j];
				b = cop2->V[v].data[j];
				res = Ai(cop2, i + 1, res + a * b);
			}
			cop2->MAC[i + 1] = res >> sf;
		}

		/* IR1 = Lm_B1[MAC1]
		   IR2 = Lm_B2[MAC2]
		   IR3 = Lm_B3[MAC3] */
		for (i = 0; i < 3; i++) {
			res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
			cop2->IR[i + 1].data = res;
		}

		/* MAC1 = A1[RBK + LR1 * IR1 + LR2 * IR2 + LR3 * IR3]
		   MAC2 = A2[GBK + LG1 * IR1 + LG2 * IR2 + LG3 * IR3]
		   MAC3 = A3[BBK + LB1 * IR1 + LB2 * IR2 + LB3 * IR3] */
		for (i = 0; i < 3; i++) {
			res = (uint64_t)(int64_t)cop2->BK.data[i] << 12;
			for (j = 0; j < 3; j++) {
				a = cop2->LCM.data[i][j];
				b = cop2->IR[j + 1].data;
				res = Ai(cop2, i + 1, res + a * b);
			}
			cop2->MAC[i + 1] = res >> sf;
		}

		/* IR1 = Lm_B1[MAC1]
		   IR2 = Lm_B2[MAC2]
		   IR3 = Lm_B3[MAC3] */
		for (i = 0; i < 3; i++) {
			res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
			cop2->IR[i + 1].data = res;
		}

		/* MAC1 = A1[R * IR1]
		   MAC2 = A2[G * IR2]
		   MAC3 = A3[B * IR3] */
		cop2->MAC[1] = ((cop2->RGBC.R << 4) * cop2->IR[1].data) >> sf;
		cop2->MAC[2] = ((cop2->RGBC.G << 4) * cop2->IR[2].data) >> sf;
		cop2->MAC[3] = ((cop2->RGBC.B << 4) * cop2->IR[3].data) >> sf;

		/* IR1 = Lm_B1[MAC1]
		   IR2 = Lm_B2[MAC2]
		   IR3 = Lm_B3[MAC3] */
		for (i = 0; i < 3; i++) {
			res = Lm_Bi(cop2, i + 1, cop2->MAC[i + 1], lm);
			cop2->IR[i + 1].data = res;
		}

		/* Cd0 <- Cd1 <- Cd2 <- CODE */
		cop2->RGB_FIFO[0].C = cop2->RGB_FIFO[1].C;
		cop2->RGB_FIFO[1].C = cop2->RGB_FIFO[2].C;
		cop2->RGB_FIFO[2].C = cop2->RGBC.C;

		/* R0 <- R1 <- R2 <- Lm_C1[MAC1]
		   G0 <- G1 <- G2 <- Lm_C2[MAC2]
		   B0 <- B1 <- B2 <- Lm_C3[MAC3] */
		for (i = 0; i < 3; i++) {
			cop2->RGB_FIFO[0].data[i] = cop2->RGB_FIFO[1].data[i];
			cop2->RGB_FIFO[1].data[i] = cop2->RGB_FIFO[2].data[i];
			res = Lm_Ci(cop2, i + 1, cop2->MAC[i + 1] >> 4);
			cop2->RGB_FIFO[2].data[i] = res;
		}
	}

	clock_consume(39);
}

void LWC2(struct r3051 *cpu)
{
	uint32_t a;
	uint32_t l;

	/* Set address and raise exception in case it is not aligned */
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;
	if (bitops_getl(&a, 0, 2) != 0)
		r3051_raise_exception(cpu, EXCEPTION_AdEL);

	/* Read data from memory and write it into coprocessor register */
	l = mem_readl(cpu, a);
	cpu->cop2.DR[cpu->instruction.i_type.rt] = l;
}

void SWC2(struct r3051 *cpu)
{
	uint32_t a;
	uint32_t l;

	/* Set address and raise exception in case it is not aligned */
	a = cpu->R[cpu->instruction.i_type.rs];
	a += (int16_t)cpu->instruction.i_type.immediate;
	if (bitops_getl(&a, 0, 2) != 0)
		r3051_raise_exception(cpu, EXCEPTION_AdEL);

	/* Read data from coprocessor register and write it to memory */
	l = cpu->cop2.DR[cpu->instruction.i_type.rt];
	mem_writel(cpu, l, a);
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
	case 0x12:
		r3051_opcode_COP2(cpu);
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
	case 0x32:
		LWC2(cpu);
		break;
	case 0x3A:
		SWC2(cpu);
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

void r3051_opcode_COP2(struct r3051 *cpu)
{
	/* Execute COP2 command if needed */
	if (cpu->instruction.cop2.imm25 == 0x25) {
		/* Reset entire flag register */
		cpu->cop2.FLAG.raw = 0;

		/* Execute command */
		switch (cpu->instruction.cop2.real_cmd) {
		case 0x01:
			RTPS(cpu);
			break;
		case 0x06:
			NCLIP(cpu);
			break;
		case 0x0C:
			OP(cpu);
			break;
		case 0x10:
			DPCS(cpu);
			break;
		case 0x11:
			INTPL(cpu);
			break;
		case 0x12:
			MVMVA(cpu);
			break;
		case 0x13:
			NCDS(cpu);
			break;
		case 0x14:
			CDP(cpu);
			break;
		case 0x16:
			NCDT(cpu);
			break;
		case 0x1B:
			NCCS(cpu);
			break;
		case 0x1C:
			CC(cpu);
			break;
		case 0x1E:
			NCS(cpu);
			break;
		case 0x20:
			NCT(cpu);
			break;
		case 0x28:
			SQR(cpu);
			break;
		case 0x29:
			DCPL(cpu);
			break;
		case 0x2A:
			DPCT(cpu);
			break;
		case 0x2D:
			AVSZ3(cpu);
			break;
		case 0x2E:
			AVSZ4(cpu);
			break;
		case 0x30:
			RTPT(cpu);
			break;
		case 0x3D:
			GPF(cpu);
			break;
		case 0x3E:
			GPL(cpu);
			break;
		case 0x3F:
			NCCT(cpu);
			break;
		default:
			LOG_W("Unknown COP2 command opcode (%02x)!\n",
				cpu->instruction.cop2.real_cmd);
			break;
		}

		/* Update error flag (bits 30..23, and 18..13 or'ed together) */
		cpu->cop2.FLAG.error =
			cpu->cop2.FLAG.mac1_larger_pos |
			cpu->cop2.FLAG.mac2_larger_pos |
			cpu->cop2.FLAG.mac3_larger_pos |
			cpu->cop2.FLAG.mac1_larger_neg |
			cpu->cop2.FLAG.mac2_larger_neg |
			cpu->cop2.FLAG.mac3_larger_neg |
			cpu->cop2.FLAG.ir1_sat |
			cpu->cop2.FLAG.ir2_sat |
			cpu->cop2.FLAG.sz3_otz_sat |
			cpu->cop2.FLAG.div_overflow |
			cpu->cop2.FLAG.mac0_larger_pos |
			cpu->cop2.FLAG.mac0_larger_neg |
			cpu->cop2.FLAG.sx2_sat |
			cpu->cop2.FLAG.sy2_sat;
	} else {
		/* Execute COP2 instruction */
		switch (cpu->instruction.cop.opcode) {
		case 0x00:
			MFC2(cpu);
			break;
		case 0x02:
			CFC2(cpu);
			break;
		case 0x04:
			MTC2(cpu);
			break;
		case 0x06:
			CTC2(cpu);
			break;
		default:
			LOG_W("Unknown COP2 opcode (%02x)!\n",
				cpu->instruction.cop.opcode);
			break;
		}
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
	int value;
	int i;

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

	/* Compute Unsigned Newton-Raphson table used for GTE division */
	for (i = 0; i <= MAX_UNR_INDEX; i++) {
		value = (0x40000 / (i + 0x100) + 1) / 2 - 0x101;
		unr_table[i] = (value < 0) ? 0 : value;
	}

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

