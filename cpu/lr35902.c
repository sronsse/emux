#include <stdio.h>
#include <stdlib.h>
#include <bitops.h>
#include <clock.h>
#include <cpu.h>
#include <log.h>
#include <memory.h>
#include <util.h>

#define DEFINE_AF_PAIR \
	union { \
		struct { \
			union { \
				uint8_t F; \
				struct lr35902_flags flags; \
			}; \
			uint8_t A; \
		}; \
		uint16_t AF; \
	};

#define DEFINE_REGISTER_PAIR(X, Y) \
	union { \
		struct { \
			uint8_t Y; \
			uint8_t X; \
		}; \
		uint16_t X##Y; \
	};

#define INT_VECTOR(irq) \
	(0x40 + (irq << 3))

struct lr35902_flags {
	uint8_t reserved:4;
	uint8_t C:1;
	uint8_t H:1;
	uint8_t N:1;
	uint8_t Z:1;
};

struct lr35902 {
	DEFINE_AF_PAIR
	DEFINE_REGISTER_PAIR(B, C)
	DEFINE_REGISTER_PAIR(D, E)
	DEFINE_REGISTER_PAIR(H, L)
	uint16_t PC;
	uint16_t SP;
	uint8_t IME;
	uint8_t IF;
	uint8_t IE;
	bool halted;
	int bus_id;
	struct clock clock;
	struct region if_region;
	struct region ie_region;
};

static bool lr35902_init(struct cpu_instance *instance);
static void lr35902_reset(struct cpu_instance *instance);
static void lr35902_interrupt(struct cpu_instance *instance, int irq);
static void lr35902_deinit(struct cpu_instance *instance);
static bool lr35902_handle_interrupts(struct lr35902 *cpu);
static void lr35902_tick(struct lr35902 *cpu);
static void lr35902_opcode_CB(struct lr35902 *cpu);
static inline void LD_r_r(struct lr35902 *cpu, uint8_t *r1, uint8_t *r2);
static inline void LD_r_n(struct lr35902 *cpu, uint8_t *r);
static inline void LD_r_cHL(struct lr35902 *cpu, uint8_t *r);
static inline void LD_cHL_r(struct lr35902 *cpu, uint8_t *r);
static inline void LD_cHL_n(struct lr35902 *cpu);
static inline void LD_A_cBC(struct lr35902 *cpu);
static inline void LD_A_cDE(struct lr35902 *cpu);
static inline void LD_A_cnn(struct lr35902 *cpu);
static inline void LD_cBC_A(struct lr35902 *cpu);
static inline void LD_cDE_A(struct lr35902 *cpu);
static inline void LD_cnn_A(struct lr35902 *cpu);
static inline void LD_A_cFF00pn(struct lr35902 *cpu);
static inline void LD_cFF00pn_A(struct lr35902 *cpu);
static inline void LD_A_cFF00pC(struct lr35902 *cpu);
static inline void LD_cFF00pC_A(struct lr35902 *cpu);
static inline void LDI_cHL_A(struct lr35902 *cpu);
static inline void LDI_A_cHL(struct lr35902 *cpu);
static inline void LDD_cHL_A(struct lr35902 *cpu);
static inline void LDD_A_cHL(struct lr35902 *cpu);
static inline void LD_rr_nn(struct lr35902 *cpu, uint16_t *rr);
static inline void LD_SP_HL(struct lr35902 *cpu);
static inline void PUSH_rr(struct lr35902 *cpu, uint16_t *rr);
static inline void POP_rr(struct lr35902 *cpu, uint16_t *rr);
static inline void POP_AF(struct lr35902 *cpu);
static inline void LD_cnn_SP(struct lr35902 *cpu);
static inline void ADD_A_r(struct lr35902 *cpu, uint8_t *r);
static inline void ADD_A_n(struct lr35902 *cpu);
static inline void ADD_A_cHL(struct lr35902 *cpu);
static inline void ADC_A_r(struct lr35902 *cpu, uint8_t *r);
static inline void ADC_A_n(struct lr35902 *cpu);
static inline void ADC_A_cHL(struct lr35902 *cpu);
static inline void SUB_A_r(struct lr35902 *cpu, uint8_t *r);
static inline void SUB_A_n(struct lr35902 *cpu);
static inline void SUB_A_cHL(struct lr35902 *cpu);
static inline void SBC_A_r(struct lr35902 *cpu, uint8_t *r);
static inline void SBC_A_n(struct lr35902 *cpu);
static inline void SBC_A_cHL(struct lr35902 *cpu);
static inline void AND_r(struct lr35902 *cpu, uint8_t *r);
static inline void AND_n(struct lr35902 *cpu);
static inline void AND_cHL(struct lr35902 *cpu);
static inline void XOR_r(struct lr35902 *cpu, uint8_t *r);
static inline void XOR_n(struct lr35902 *cpu);
static inline void XOR_cHL(struct lr35902 *cpu);
static inline void OR_r(struct lr35902 *cpu, uint8_t *r);
static inline void OR_n(struct lr35902 *cpu);
static inline void OR_cHL(struct lr35902 *cpu);
static inline void CP_r(struct lr35902 *cpu, uint8_t *r);
static inline void CP_n(struct lr35902 *cpu);
static inline void CP_cHL(struct lr35902 *cpu);
static inline void INC_r(struct lr35902 *cpu, uint8_t *r);
static inline void INC_cHL(struct lr35902 *cpu);
static inline void DEC_r(struct lr35902 *cpu, uint8_t *r);
static inline void DEC_cHL(struct lr35902 *cpu);
static inline void DAA(struct lr35902 *cpu);
static inline void CPL(struct lr35902 *cpu);
static inline void ADD_HL_rr(struct lr35902 *cpu, uint16_t *rr);
static inline void INC_rr(struct lr35902 *cpu, uint16_t *rr);
static inline void DEC_rr(struct lr35902 *cpu, uint16_t *rr);
static inline void ADD_SP_d(struct lr35902 *cpu);
static inline void LD_HL_SPpd(struct lr35902 *cpu);
static inline void RLCA(struct lr35902 *cpu);
static inline void RLA(struct lr35902 *cpu);
static inline void RRCA(struct lr35902 *cpu);
static inline void RRA(struct lr35902 *cpu);
static inline void RLC_r(struct lr35902 *cpu, uint8_t *r);
static inline void RLC_cHL(struct lr35902 *cpu);
static inline void RL_r(struct lr35902 *cpu, uint8_t *r);
static inline void RL_cHL(struct lr35902 *cpu);
static inline void RRC_r(struct lr35902 *cpu, uint8_t *r);
static inline void RRC_cHL(struct lr35902 *cpu);
static inline void RR_r(struct lr35902 *cpu, uint8_t *r);
static inline void RR_cHL(struct lr35902 *cpu);
static inline void SWAP_r(struct lr35902 *cpu, uint8_t *r);
static inline void SWAP_cHL(struct lr35902 *cpu);
static inline void SRA_r(struct lr35902 *cpu, uint8_t *r);
static inline void SRA_cHL(struct lr35902 *cpu);
static inline void SLA_r(struct lr35902 *cpu, uint8_t *r);
static inline void SLA_cHL(struct lr35902 *cpu);
static inline void SRL_r(struct lr35902 *cpu, uint8_t *r);
static inline void SRL_cHL(struct lr35902 *cpu);
static inline void BIT_n_r(struct lr35902 *cpu, uint8_t n, uint8_t *r);
static inline void BIT_n_cHL(struct lr35902 *cpu, uint8_t n);
static inline void SET_n_r(struct lr35902 *cpu, uint8_t n, uint8_t *r);
static inline void SET_n_cHL(struct lr35902 *cpu, uint8_t n);
static inline void RES_n_r(struct lr35902 *cpu, uint8_t n, uint8_t *r);
static inline void RES_n_cHL(struct lr35902 *cpu, uint8_t n);
static inline void CCF(struct lr35902 *cpu);
static inline void SCF(struct lr35902 *cpu);
static inline void NOP(struct lr35902 *cpu);
static inline void HALT(struct lr35902 *cpu);
static inline void STOP(struct lr35902 *cpu);
static inline void DI(struct lr35902 *cpu);
static inline void EI(struct lr35902 *cpu);
static inline void JP_nn(struct lr35902 *cpu);
static inline void JP_HL(struct lr35902 *cpu);
static inline void JP_f_nn(struct lr35902 *cpu, bool condition);
static inline void JP_NZ_nn(struct lr35902 *cpu);
static inline void JP_Z_nn(struct lr35902 *cpu);
static inline void JP_NC_nn(struct lr35902 *cpu);
static inline void JP_C_nn(struct lr35902 *cpu);
static inline void JR_d(struct lr35902 *cpu);
static inline void JR_f_d(struct lr35902 *cpu, bool condition);
static inline void JR_NZ_d(struct lr35902 *cpu);
static inline void JR_Z_d(struct lr35902 *cpu);
static inline void JR_NC_d(struct lr35902 *cpu);
static inline void JR_C_d(struct lr35902 *cpu);
static inline void CALL_nn(struct lr35902 *cpu);
static inline void CALL_f_nn(struct lr35902 *cpu, bool condition);
static inline void CALL_NZ_nn(struct lr35902 *cpu);
static inline void CALL_Z_nn(struct lr35902 *cpu);
static inline void CALL_NC_nn(struct lr35902 *cpu);
static inline void CALL_C_nn(struct lr35902 *cpu);
static inline void RET(struct lr35902 *cpu);
static inline void RET_f(struct lr35902 *cpu, bool condition);
static inline void RET_NZ(struct lr35902 *cpu);
static inline void RET_Z(struct lr35902 *cpu);
static inline void RET_NC(struct lr35902 *cpu);
static inline void RET_C(struct lr35902 *cpu);
static inline void RETI(struct lr35902 *cpu);
static inline void RST_n(struct lr35902 *cpu, uint8_t n);

void LD_r_r(struct lr35902 *UNUSED(cpu), uint8_t *r1, uint8_t *r2)
{
	*r1 = *r2;
	clock_consume(4);
}

void LD_r_n(struct lr35902 *cpu, uint8_t *r)
{
	*r = memory_readb(cpu->bus_id, cpu->PC++);
	clock_consume(8);
}

void LD_r_cHL(struct lr35902 *cpu, uint8_t *r)
{
	*r = memory_readb(cpu->bus_id, cpu->HL);
	clock_consume(8);
}

void LD_cHL_r(struct lr35902 *cpu, uint8_t *r)
{
	memory_writeb(cpu->bus_id, *r, cpu->HL);
	clock_consume(8);
}

void LD_cHL_n(struct lr35902 *cpu)
{
	memory_writeb(cpu->bus_id, memory_readb(cpu->bus_id, cpu->PC++),
		cpu->HL);
	clock_consume(12);
}

void LD_A_cBC(struct lr35902 *cpu)
{
	cpu->A = memory_readb(cpu->bus_id, cpu->BC);
	clock_consume(8);
}

void LD_A_cDE(struct lr35902 *cpu)
{
	cpu->A = memory_readb(cpu->bus_id, cpu->DE);
	clock_consume(8);
}

void LD_A_cnn(struct lr35902 *cpu)
{
	uint16_t address = memory_readb(cpu->bus_id, cpu->PC++);
	address |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	cpu->A = memory_readb(cpu->bus_id, address);
	clock_consume(16);
}

void LD_cBC_A(struct lr35902 *cpu)
{
	memory_writeb(cpu->bus_id, cpu->A, cpu->BC);
	clock_consume(8);
}

void LD_cDE_A(struct lr35902 *cpu)
{
	memory_writeb(cpu->bus_id, cpu->A, cpu->DE);
	clock_consume(8);
}

void LD_cnn_A(struct lr35902 *cpu)
{
	uint16_t address = memory_readb(cpu->bus_id, cpu->PC++);
	address |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	memory_writeb(cpu->bus_id, cpu->A, address);
	clock_consume(16);
}

void LD_A_cFF00pn(struct lr35902 *cpu)
{
	cpu->A = memory_readb(cpu->bus_id, 0xFF00 +
		memory_readb(cpu->bus_id, cpu->PC++));
	clock_consume(12);
}

void LD_cFF00pn_A(struct lr35902 *cpu)
{
	memory_writeb(cpu->bus_id, cpu->A, 0xFF00 +
		memory_readb(cpu->bus_id, cpu->PC++));
	clock_consume(12);
}

void LD_A_cFF00pC(struct lr35902 *cpu)
{
	cpu->A = memory_readb(cpu->bus_id, 0xFF00 + cpu->C);
	clock_consume(8);
}

void LD_cFF00pC_A(struct lr35902 *cpu)
{
	memory_writeb(cpu->bus_id, cpu->A, 0xFF00 + cpu->C);
	clock_consume(8);
}

void LDI_cHL_A(struct lr35902 *cpu)
{
	memory_writeb(cpu->bus_id, cpu->A, cpu->HL++);
	clock_consume(8);
}

void LDI_A_cHL(struct lr35902 *cpu)
{
	cpu->A = memory_readb(cpu->bus_id, cpu->HL++);
	clock_consume(8);
}

void LDD_cHL_A(struct lr35902 *cpu)
{
	memory_writeb(cpu->bus_id, cpu->A, cpu->HL--);
	clock_consume(8);
}

void LDD_A_cHL(struct lr35902 *cpu)
{
	cpu->A = memory_readb(cpu->bus_id, cpu->HL--);
	clock_consume(8);
}

void LD_rr_nn(struct lr35902 *cpu, uint16_t *rr)
{
	uint16_t nn = memory_readb(cpu->bus_id, cpu->PC++);
	nn |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	*rr = nn;
	clock_consume(12);
}

void LD_SP_HL(struct lr35902 *cpu)
{
	cpu->SP = cpu->HL;
	clock_consume(8);
}

void PUSH_rr(struct lr35902 *cpu, uint16_t *rr)
{
	memory_writeb(cpu->bus_id, *rr >> 8, --cpu->SP);
	memory_writeb(cpu->bus_id, *rr, --cpu->SP);
	clock_consume(16);
}

void POP_rr(struct lr35902 *cpu, uint16_t *rr)
{
	*rr = memory_readb(cpu->bus_id, cpu->SP++);
	*rr |= memory_readb(cpu->bus_id, cpu->SP++) << 8;
	clock_consume(12);
}

void POP_AF(struct lr35902 *cpu)
{
	POP_rr(cpu, &cpu->AF);
	cpu->flags.reserved = 0;
	clock_consume(12);
}

void LD_cnn_SP(struct lr35902 *cpu)
{
	uint16_t nn = memory_readb(cpu->bus_id, cpu->PC++);
	nn |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	memory_writeb(cpu->bus_id, cpu->SP, nn);
	memory_writeb(cpu->bus_id, cpu->SP >> 8, nn + 1);
	clock_consume(20);
}

void ADD_A_r(struct lr35902 *cpu, uint8_t *r)
{
	uint16_t result = cpu->A + *r;
	cpu->flags.C = result >> 8;
	cpu->flags.H = ((cpu->A & 0x0F) + (*r & 0x0F) > 0x0F);
	cpu->flags.N = 0;
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->A = result;
	clock_consume(4);
}

void ADD_A_n(struct lr35902 *cpu)
{
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t result = cpu->A + n;
	cpu->flags.C = result >> 8;
	cpu->flags.H = ((cpu->A & 0x0F) + (n & 0x0F) > 0x0F);
	cpu->flags.N = 0;
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->A = result;
	clock_consume(8);
}

void ADD_A_cHL(struct lr35902 *cpu)
{
	uint16_t result = cpu->A + memory_readb(cpu->bus_id, cpu->HL);
	cpu->flags.C = result >> 8;
	cpu->flags.H = ((cpu->A & 0x0F) +
		(memory_readb(cpu->bus_id, cpu->HL) & 0x0F) > 0x0F);
	cpu->flags.N = 0;
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->A = result;
	clock_consume(8);
}

void ADC_A_r(struct lr35902 *cpu, uint8_t *r)
{
	uint16_t result = cpu->A + *r + cpu->flags.C;
	cpu->flags.H = ((cpu->A & 0x0F) + (*r & 0x0F) + cpu->flags.C > 0x0F);
	cpu->flags.C = result >> 8;
	cpu->flags.N = 0;
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->A = result;
	clock_consume(4);
}

void ADC_A_n(struct lr35902 *cpu)
{
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t result = cpu->A + n + cpu->flags.C;
	cpu->flags.H = ((cpu->A & 0x0F) + (n & 0x0F) + cpu->flags.C > 0x0F);
	cpu->flags.C = result >> 8;
	cpu->flags.N = 0;
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->A = result;
	clock_consume(8);
}

void ADC_A_cHL(struct lr35902 *cpu)
{
	uint16_t result = cpu->A + memory_readb(cpu->bus_id, cpu->HL) +
		cpu->flags.C;
	cpu->flags.H = ((cpu->A & 0x0F) +
		(memory_readb(cpu->bus_id, cpu->HL) & 0x0F) +
		cpu->flags.C > 0x0F);
	cpu->flags.C = result >> 8;
	cpu->flags.N = 0;
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->A = result;
	clock_consume(8);
}

void SUB_A_r(struct lr35902 *cpu, uint8_t *r)
{
	int16_t result = cpu->A - *r;
	cpu->flags.C = result >> 8;
	cpu->flags.H = ((cpu->A & 0x0F) - (*r & 0x0F) < 0);
	cpu->flags.N = 1;
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->A = result;
	clock_consume(4);
}

void SUB_A_n(struct lr35902 *cpu)
{
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	int16_t result = cpu->A - n;
	cpu->flags.C = result >> 8;
	cpu->flags.H = ((cpu->A & 0x0F) - (n & 0x0F) < 0);
	cpu->flags.N = 1;
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->A = result;
	clock_consume(8);
}

void SUB_A_cHL(struct lr35902 *cpu)
{
	int16_t result = cpu->A - memory_readb(cpu->bus_id, cpu->HL);
	cpu->flags.C = result >> 8;
	cpu->flags.H = ((cpu->A & 0x0F) -
		(memory_readb(cpu->bus_id, cpu->HL) & 0x0F) < 0);
	cpu->flags.N = 1;
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->A = result;
	clock_consume(8);
}

void SBC_A_r(struct lr35902 *cpu, uint8_t *r)
{
	int16_t result = cpu->A - *r - cpu->flags.C;
	cpu->flags.H = ((cpu->A & 0x0F) - (*r & 0x0F) - cpu->flags.C < 0);
	cpu->flags.C = result >> 8;
	cpu->flags.N = 1;
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->A = result;
	clock_consume(4);
}

void SBC_A_n(struct lr35902 *cpu)
{
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	int16_t result = cpu->A - n - cpu->flags.C;
	cpu->flags.H = ((cpu->A & 0x0F) - (n & 0x0F) - cpu->flags.C < 0);
	cpu->flags.C = result >> 8;
	cpu->flags.N = 1;
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->A = result;
	clock_consume(8);
}

void SBC_A_cHL(struct lr35902 *cpu)
{
	int16_t result = cpu->A - memory_readb(cpu->bus_id, cpu->HL) -
		cpu->flags.C;
	cpu->flags.H = ((cpu->A & 0x0F) -
		(memory_readb(cpu->bus_id, cpu->HL) & 0x0F) - cpu->flags.C < 0);
	cpu->flags.C = result >> 8;
	cpu->flags.N = 1;
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->A = result;
	clock_consume(8);
}

void AND_r(struct lr35902 *cpu, uint8_t *r)
{
	cpu->A &= *r;
	cpu->flags.C = 0;
	cpu->flags.H = 1;
	cpu->flags.N = 0;
	cpu->flags.Z = (cpu->A == 0);
	clock_consume(4);
}

void AND_n(struct lr35902 *cpu)
{
	cpu->A &= memory_readb(cpu->bus_id, cpu->PC++);
	cpu->flags.C = 0;
	cpu->flags.H = 1;
	cpu->flags.N = 0;
	cpu->flags.Z = (cpu->A == 0);
	clock_consume(8);
}

void AND_cHL(struct lr35902 *cpu)
{
	cpu->A &= memory_readb(cpu->bus_id, cpu->HL);
	cpu->flags.C = 0;
	cpu->flags.H = 1;
	cpu->flags.N = 0;
	cpu->flags.Z = (cpu->A == 0);
	clock_consume(8);
}

void XOR_r(struct lr35902 *cpu, uint8_t *r)
{
	cpu->A ^= *r;
	cpu->flags.C = 0;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = (cpu->A == 0);
	clock_consume(4);
}

void XOR_n(struct lr35902 *cpu)
{
	cpu->A ^= memory_readb(cpu->bus_id, cpu->PC++);
	cpu->flags.C = 0;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = (cpu->A == 0);
	clock_consume(8);
}

void XOR_cHL(struct lr35902 *cpu)
{
	cpu->A ^= memory_readb(cpu->bus_id, cpu->HL);
	cpu->flags.C = 0;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = (cpu->A == 0);
	clock_consume(8);
}

void OR_r(struct lr35902 *cpu, uint8_t *r)
{
	cpu->A |= *r;
	cpu->flags.C = 0;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = (cpu->A == 0);
	clock_consume(4);
}

void OR_n(struct lr35902 *cpu)
{
	cpu->A |= memory_readb(cpu->bus_id, cpu->PC++);
	cpu->flags.C = 0;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = (cpu->A == 0);
	clock_consume(8);
}

void OR_cHL(struct lr35902 *cpu)
{
	cpu->A |= memory_readb(cpu->bus_id, cpu->HL);
	cpu->flags.C = 0;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = (cpu->A == 0);
	clock_consume(8);
}

void CP_r(struct lr35902 *cpu, uint8_t *r)
{
	int16_t result = cpu->A - *r;
	cpu->flags.C = result >> 8;
	cpu->flags.H = ((cpu->A & 0x0F) - (*r & 0x0F) < 0);
	cpu->flags.N = 1;
	cpu->flags.Z = ((uint8_t)result == 0);
	clock_consume(4);
}

void CP_n(struct lr35902 *cpu)
{
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	int16_t result = cpu->A - n;
	cpu->flags.C = result >> 8;
	cpu->flags.H = ((cpu->A & 0x0F) - (n & 0x0F) < 0);
	cpu->flags.N = 1;
	cpu->flags.Z = ((uint8_t)result == 0);
	clock_consume(8);
}

void CP_cHL(struct lr35902 *cpu)
{
	int16_t result = cpu->A - memory_readb(cpu->bus_id, cpu->HL);
	cpu->flags.C = result >> 8;
	cpu->flags.H = ((cpu->A & 0x0F) -
		(memory_readb(cpu->bus_id, cpu->HL) & 0x0F) < 0);
	cpu->flags.N = 1;
	cpu->flags.Z = ((uint8_t)result == 0);
	clock_consume(8);
}

void INC_r(struct lr35902 *cpu, uint8_t *r)
{
	cpu->flags.H = ((*r & 0x0F) == 0x0F);
	cpu->flags.N = 0;
	cpu->flags.Z = (*r == 0xFF);
	(*r)++;
	clock_consume(4);
}

void INC_cHL(struct lr35902 *cpu)
{
	cpu->flags.H = ((memory_readb(cpu->bus_id, cpu->HL) & 0x0F) == 0x0F);
	cpu->flags.N = 0;
	cpu->flags.Z = (memory_readb(cpu->bus_id, cpu->HL) == 0xFF);
	memory_writeb(cpu->bus_id, memory_readb(cpu->bus_id, cpu->HL) + 1,
		cpu->HL);
	clock_consume(12);
}

void DEC_r(struct lr35902 *cpu, uint8_t *r)
{
	(*r)--;
	cpu->flags.H = ((*r & 0x0F) == 0x0F);
	cpu->flags.N = 1;
	cpu->flags.Z = (*r == 0);
	clock_consume(4);
}

void DEC_cHL(struct lr35902 *cpu)
{
	memory_writeb(cpu->bus_id, memory_readb(cpu->bus_id, cpu->HL) - 1,
		cpu->HL);
	cpu->flags.H = ((memory_readb(cpu->bus_id, cpu->HL) & 0x0F) == 0x0F);
	cpu->flags.N = 1;
	cpu->flags.Z = (memory_readb(cpu->bus_id, cpu->HL) == 0);
	clock_consume(12);
}

void DAA(struct lr35902 *cpu)
{
	int a = cpu->A;

	/* Update A based on N/H/C flags */
	if (!cpu->flags.N) {
		if (cpu->flags.H || ((a & 0x0F) > 9))
			a += 6;
		if (cpu->flags.C || (a > 0x9F))
			a += 0x60;
	} else {
		if (cpu->flags.H)
			a = (cpu->A - 6) & 0xFF;
		if (cpu->flags.C)
			a -= 0x60;
	}

	/* Reset half-carry and zero flags */
	cpu->flags.H = 0;
	cpu->flags.Z = 0;

	/* Set carry flag if needed */
	if (a & 0x100)
		cpu->flags.C = 1;

	/* Save A register */
	cpu->A = a & 0xFF;

	/* Set zero flag if needed */
	if (!cpu->A)
		cpu->flags.Z = 1;

	/* Consume cycles */
	clock_consume(4);
}

void CPL(struct lr35902 *cpu)
{
	cpu->A = ~cpu->A;
	cpu->flags.H = 1;
	cpu->flags.N = 1;
	clock_consume(4);
}

void ADD_HL_rr(struct lr35902 *cpu, uint16_t *rr)
{
	uint32_t result = cpu->HL + *rr;
	cpu->flags.C = result >> 16;
	cpu->flags.H = ((cpu->HL & 0x0FFF) + (*rr & 0x0FFF) > 0x0FFF);
	cpu->flags.N = 0;
	cpu->HL = result;
	clock_consume(8);
}

void INC_rr(struct lr35902 *UNUSED(cpu), uint16_t *rr)
{
	(*rr)++;
	clock_consume(8);
}

void DEC_rr(struct lr35902 *UNUSED(cpu), uint16_t *rr)
{
	(*rr)--;
	clock_consume(8);
}

void ADD_SP_d(struct lr35902 *cpu)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	int32_t result = cpu->SP + d;
	cpu->flags.C = result >> 16;
	cpu->flags.H = ((cpu->SP & 0x0FFF) + (d & 0x0FFF) > 0x0FFF);
	cpu->flags.N = 0;
	cpu->flags.Z = 0;
	cpu->SP = result;
	clock_consume(16);
}

void LD_HL_SPpd(struct lr35902 *cpu)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint32_t acc = (uint32_t)cpu->SP + (uint32_t)d;
	cpu->F = (0x20 & (((cpu->SP>>8) ^ ((d)>>8) ^ (acc >> 8)) << 1));
	cpu->flags.C = (acc >> 16);
	cpu->HL = acc;
	clock_consume(12);
}

void RLCA(struct lr35902 *cpu)
{
	cpu->flags.C = ((cpu->A & 0x80) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = 0;
	cpu->A = (cpu->A << 1) | cpu->flags.C;
	clock_consume(4);
}

void RLA(struct lr35902 *cpu)
{
	int old_carry = cpu->flags.C;
	cpu->flags.C = ((cpu->A & 0x80) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = 0;
	cpu->A = (cpu->A << 1) | old_carry;
	clock_consume(4);
}

void RRCA(struct lr35902 *cpu)
{
	cpu->flags.C = ((cpu->A & 0x01) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = 0;
	cpu->A = (cpu->A >> 1) | (cpu->flags.C << 7);
	clock_consume(4);
}

void RRA(struct lr35902 *cpu)
{
	int old_carry = cpu->flags.C;
	cpu->flags.C = ((cpu->A & 0x01) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = 0;
	cpu->A = (cpu->A >> 1) | (old_carry << 7);
	clock_consume(4);
}

void RLC_r(struct lr35902 *cpu, uint8_t *r)
{
	cpu->flags.C = ((*r & 0x80) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = (*r == 0);
	*r = (*r << 1) | cpu->flags.C;
	clock_consume(8);
}

void RLC_cHL(struct lr35902 *cpu)
{
	cpu->flags.C = ((memory_readb(cpu->bus_id, cpu->HL) & 0x80) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	memory_writeb(cpu->bus_id,
		(memory_readb(cpu->bus_id, cpu->HL) << 1) | cpu->flags.C,
		cpu->HL);
	cpu->flags.Z = (memory_readb(cpu->bus_id, cpu->HL) == 0);
	clock_consume(16);
}

void RL_r(struct lr35902 *cpu, uint8_t *r)
{
	int old_carry = cpu->flags.C;
	cpu->flags.C = ((*r & 0x80) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	*r = (*r << 1) | old_carry;
	cpu->flags.Z = (*r == 0);
	clock_consume(8);
}

void RL_cHL(struct lr35902 *cpu)
{
	int old_carry = cpu->flags.C;
	cpu->flags.C = ((memory_readb(cpu->bus_id, cpu->HL) & 0x80) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	memory_writeb(cpu->bus_id,
		(memory_readb(cpu->bus_id, cpu->HL) << 1) | old_carry,
		cpu->HL);
	cpu->flags.Z = (memory_readb(cpu->bus_id, cpu->HL) == 0);
	clock_consume(16);
}

void RRC_r(struct lr35902 *cpu, uint8_t *r)
{
	cpu->flags.C = ((*r & 0x01) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = (*r == 0);
	*r = (*r >> 1) | (cpu->flags.C << 7);
	clock_consume(8);
}

void RRC_cHL(struct lr35902 *cpu)
{
	cpu->flags.C = ((memory_readb(cpu->bus_id, cpu->HL) & 0x01) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = (memory_readb(cpu->bus_id, cpu->HL) == 0);
	memory_writeb(cpu->bus_id,
		(memory_readb(cpu->bus_id, cpu->HL) >> 1) | (cpu->flags.C << 7),
		cpu->HL);
	clock_consume(16);
}

void RR_r(struct lr35902 *cpu, uint8_t *r)
{
	int old_carry = cpu->flags.C;
	cpu->flags.C = ((*r & 0x01) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	*r = (*r >> 1) | (old_carry << 7);
	cpu->flags.Z = (*r == 0);
	clock_consume(8);
}

void RR_cHL(struct lr35902 *cpu)
{
	int old_carry = cpu->flags.C;
	cpu->flags.C = ((memory_readb(cpu->bus_id, cpu->HL) & 0x01) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	memory_writeb(cpu->bus_id,
		(memory_readb(cpu->bus_id, cpu->HL) >> 1) | (old_carry << 7),
		cpu->HL);
	cpu->flags.Z = (memory_readb(cpu->bus_id, cpu->HL) == 0);
	clock_consume(16);
}

void SWAP_r(struct lr35902 *cpu, uint8_t *r)
{
	*r = (*r << 4) | (*r >> 4);
	cpu->flags.C = 0;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = (*r == 0);
	clock_consume(8);
}

void SWAP_cHL(struct lr35902 *cpu)
{
	memory_writeb(cpu->bus_id,
		(memory_readb(cpu->bus_id, cpu->HL) << 4) |
		(memory_readb(cpu->bus_id, cpu->HL) >> 4),
		cpu->HL);
	cpu->flags.C = 0;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.Z = (memory_readb(cpu->bus_id, cpu->HL) == 0);
	clock_consume(16);
}

void SRA_r(struct lr35902 *cpu, uint8_t *r)
{
	cpu->flags.C = ((*r & 0x01) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	*r = (*r >> 1) | (*r & 0x80);
	cpu->flags.Z = (*r == 0);
	clock_consume(8);
}

void SRA_cHL(struct lr35902 *cpu)
{
	cpu->flags.C = ((memory_readb(cpu->bus_id, cpu->HL) & 0x01) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	memory_writeb(cpu->bus_id,
		(memory_readb(cpu->bus_id, cpu->HL) >> 1) |
		(memory_readb(cpu->bus_id, cpu->HL) & 0x80),
		cpu->HL);
	cpu->flags.Z = (memory_readb(cpu->bus_id, cpu->HL) == 0);
	clock_consume(16);
}

void SLA_r(struct lr35902 *cpu, uint8_t *r)
{
	cpu->flags.C = ((*r & 0x80) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	*r <<= 1;
	cpu->flags.Z = (*r == 0);
	clock_consume(8);
}

void SLA_cHL(struct lr35902 *cpu)
{
	cpu->flags.C = ((memory_readb(cpu->bus_id, cpu->HL) & 0x80) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	memory_writeb(cpu->bus_id, memory_readb(cpu->bus_id, cpu->HL) << 1,
		cpu->HL);
	cpu->flags.Z = (memory_readb(cpu->bus_id, cpu->HL) == 0);
	clock_consume(16);
}

void SRL_r(struct lr35902 *cpu, uint8_t *r)
{
	cpu->flags.C = ((*r & 0x01) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	*r >>= 1;
	cpu->flags.Z = (*r == 0);
	clock_consume(8);
}

void SRL_cHL(struct lr35902 *cpu)
{
	cpu->flags.C = ((memory_readb(cpu->bus_id, cpu->HL) & 0x01) != 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	memory_writeb(cpu->bus_id, memory_readb(cpu->bus_id, cpu->HL) >> 1,
		 cpu->HL);
	cpu->flags.Z = (memory_readb(cpu->bus_id, cpu->HL) == 0);
	clock_consume(16);
}

void BIT_n_r(struct lr35902 *cpu, uint8_t n, uint8_t *r)
{
	cpu->flags.H = 1;
	cpu->flags.N = 0;
	cpu->flags.Z = ((*r & (1 << n)) == 0);
	clock_consume(8);
}

void BIT_n_cHL(struct lr35902 *cpu, uint8_t n)
{
	cpu->flags.H = 1;
	cpu->flags.N = 0;
	cpu->flags.Z = ((memory_readb(cpu->bus_id, cpu->HL) & (1 << n)) == 0);
	clock_consume(12);
}

void SET_n_r(struct lr35902 *UNUSED(cpu), uint8_t n, uint8_t *r)
{
	*r |= (1 << n);
	clock_consume(8);
}

void SET_n_cHL(struct lr35902 *cpu, uint8_t n)
{
	memory_writeb(cpu->bus_id,
		memory_readb(cpu->bus_id, cpu->HL) | (1 << n),
		cpu->HL);
	clock_consume(16);
}

void RES_n_r(struct lr35902 *UNUSED(cpu), uint8_t n, uint8_t *r)
{
	*r &= ~(1 << n);
	clock_consume(8);
}

void RES_n_cHL(struct lr35902 *cpu, uint8_t n)
{
	memory_writeb(cpu->bus_id,
		memory_readb(cpu->bus_id, cpu->HL) & ~(1 << n),
		cpu->HL);
	clock_consume(16);
}

void CCF(struct lr35902 *cpu)
{
	cpu->flags.C = !cpu->flags.C;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	clock_consume(4);
}

void SCF(struct lr35902 *cpu)
{
	cpu->flags.C = 1;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	clock_consume(4);
}

void NOP(struct lr35902 *UNUSED(cpu))
{
	clock_consume(4);
}

void HALT(struct lr35902 *cpu)
{
	cpu->halted = true;
	clock_consume(4);
}

void STOP(struct lr35902 *cpu)
{
	memory_readb(cpu->bus_id, cpu->PC++);
	cpu->halted = true;
	clock_consume(4);
}

void DI(struct lr35902 *cpu)
{
	cpu->IME = 0;
	clock_consume(4);
}

void EI(struct lr35902 *cpu)
{
	cpu->IME = 1;
	clock_consume(4);
}

void JP_nn(struct lr35902 *cpu)
{
	uint8_t n1 = memory_readb(cpu->bus_id, cpu->PC++);
	uint8_t n2 = memory_readb(cpu->bus_id, cpu->PC++);
	cpu->PC = n1 | (n2 << 8);
	clock_consume(16);
}

void JP_HL(struct lr35902 *cpu)
{
	cpu->PC = cpu->HL;
	clock_consume(4);
}

void JP_f_nn(struct lr35902 *cpu, bool condition)
{
	uint8_t n1 = memory_readb(cpu->bus_id, cpu->PC++);
	uint8_t n2 = memory_readb(cpu->bus_id, cpu->PC++);
	if (condition) {
		cpu->PC = n1 | (n2 << 8);
		clock_consume(4);
	}
	clock_consume(12);
}

void JP_NZ_nn(struct lr35902 *cpu)
{
	JP_f_nn(cpu, !cpu->flags.Z);
}

void JP_Z_nn(struct lr35902 *cpu)
{
	JP_f_nn(cpu, cpu->flags.Z);
}

void JP_NC_nn(struct lr35902 *cpu)
{
	JP_f_nn(cpu, !cpu->flags.C);
}

void JP_C_nn(struct lr35902 *cpu)
{
	JP_f_nn(cpu, cpu->flags.C);
}

void JR_d(struct lr35902 *cpu)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	cpu->PC += d;
	clock_consume(12);
}

void JR_f_d(struct lr35902 *cpu, bool condition)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	if (condition) {
		cpu->PC += d;
		clock_consume(4);
	}
	clock_consume(8);
}

void JR_NZ_d(struct lr35902 *cpu)
{
	JR_f_d(cpu, !cpu->flags.Z);
}

void JR_Z_d(struct lr35902 *cpu)
{
	JR_f_d(cpu, cpu->flags.Z);
}

void JR_NC_d(struct lr35902 *cpu)
{
	JR_f_d(cpu, !cpu->flags.C);
}

void JR_C_d(struct lr35902 *cpu)
{
	JR_f_d(cpu, cpu->flags.C);
}

void CALL_nn(struct lr35902 *cpu)
{
	uint8_t n1 = memory_readb(cpu->bus_id, cpu->PC++);
	uint8_t n2 = memory_readb(cpu->bus_id, cpu->PC++);
	memory_writeb(cpu->bus_id, cpu->PC >> 8, --cpu->SP);
	memory_writeb(cpu->bus_id, cpu->PC, --cpu->SP);
	cpu->PC = n1 | (n2 << 8);
	clock_consume(24);
}

void CALL_f_nn(struct lr35902 *cpu, bool condition)
{
	uint8_t n1 = memory_readb(cpu->bus_id, cpu->PC++);
	uint8_t n2 = memory_readb(cpu->bus_id, cpu->PC++);
	if (condition) {
		memory_writeb(cpu->bus_id, cpu->PC >> 8, --cpu->SP);
		memory_writeb(cpu->bus_id, cpu->PC, --cpu->SP);
		cpu->PC = n1 | (n2 << 8);
		clock_consume(12);
	}
	clock_consume(12);
}

void CALL_NZ_nn(struct lr35902 *cpu)
{
	CALL_f_nn(cpu, !cpu->flags.Z);
}

void CALL_Z_nn(struct lr35902 *cpu)
{
	CALL_f_nn(cpu, cpu->flags.Z);
}

void CALL_NC_nn(struct lr35902 *cpu)
{
	CALL_f_nn(cpu, !cpu->flags.C);
}

void CALL_C_nn(struct lr35902 *cpu)
{
	CALL_f_nn(cpu, cpu->flags.C);
}

void RET(struct lr35902 *cpu)
{
	cpu->PC = memory_readb(cpu->bus_id, cpu->SP++);
	cpu->PC |= memory_readb(cpu->bus_id, cpu->SP++) << 8;
	clock_consume(16);
}

void RET_f(struct lr35902 *cpu, bool condition)
{
	if (condition) {
		cpu->PC = memory_readb(cpu->bus_id, cpu->SP++);
		cpu->PC |= memory_readb(cpu->bus_id, cpu->SP++) << 8;
		clock_consume(12);
	}
	clock_consume(8);
}

void RET_NZ(struct lr35902 *cpu)
{
	RET_f(cpu, !cpu->flags.Z);
}

void RET_Z(struct lr35902 *cpu)
{
	RET_f(cpu, cpu->flags.Z);
}

void RET_NC(struct lr35902 *cpu)
{
	RET_f(cpu, !cpu->flags.C);
}

void RET_C(struct lr35902 *cpu)
{
	RET_f(cpu, cpu->flags.C);
}

void RETI(struct lr35902 *cpu)
{
	cpu->PC = memory_readb(cpu->bus_id, cpu->SP++);
	cpu->PC |= memory_readb(cpu->bus_id, cpu->SP++) << 8;
	cpu->IME = 1;
	clock_consume(16);
}

void RST_n(struct lr35902 *cpu, uint8_t n)
{
	memory_writeb(cpu->bus_id, cpu->PC >> 8, --cpu->SP);
	memory_writeb(cpu->bus_id, cpu->PC, --cpu->SP);
	cpu->PC = n;
	clock_consume(16);
}

bool lr35902_handle_interrupts(struct lr35902 *cpu)
{
	int irq;

	/* Get interrupt request (by priority) and leave if none is active */
	irq = bitops_ffs(cpu->IF);
	if (irq-- == 0)
		return false;

	/* Any interrupt should resume CPU (regardless of IME flag) */
	cpu->halted = 0;

	/* Check if interrupts are enabled */
	if (!cpu->IME)
		return false;

	/* Check if particular interrupt is enabled */
	if (!(cpu->IE & BIT(irq)))
		return false;

	/* Clear master interrupt enable flag */
	cpu->IME = 0;

	/* Clear interrupt request flag */
	cpu->IF &= ~BIT(irq);

	/* Push PC on stack */
	memory_writeb(cpu->bus_id, cpu->PC >> 8, --cpu->SP);
	memory_writeb(cpu->bus_id, cpu->PC, --cpu->SP);

	/* Jump to interrupt address */
	cpu->PC = INT_VECTOR(irq);

	/* Interrupt handler should consume 20 cycles */
	clock_consume(20);
	return true;
}

void lr35902_tick(struct lr35902 *cpu)
{
	uint8_t opcode;

	/* Check for interrupt requests */
	if (lr35902_handle_interrupts(cpu))
		return;

	/* Check if CPU is halted */
	if (cpu->halted) {
		clock_consume(1);
		return;
	}

	/* Fetch opcode */
	opcode = memory_readb(cpu->bus_id, cpu->PC++);

	/* Execute opcode */
	switch (opcode) {
	case 0x00:
		NOP(cpu);
		break;
	case 0x01:
		LD_rr_nn(cpu, &cpu->BC);
		break;
	case 0x02:
		LD_cBC_A(cpu);
		break;
	case 0x03:
		INC_rr(cpu, &cpu->BC);
		break;
	case 0x04:
		INC_r(cpu, &cpu->B);
		break;
	case 0x05:
		DEC_r(cpu, &cpu->B);
		break;
	case 0x06:
		LD_r_n(cpu, &cpu->B);
		break;
	case 0x07:
		RLCA(cpu);
		break;
	case 0x08:
		LD_cnn_SP(cpu);
		break;
	case 0x09:
		ADD_HL_rr(cpu, &cpu->BC);
		break;
	case 0x0A:
		LD_A_cBC(cpu);
		break;
	case 0x0B:
		DEC_rr(cpu, &cpu->BC);
		break;
	case 0x0C:
		INC_r(cpu, &cpu->C);
		break;
	case 0x0D:
		DEC_r(cpu, &cpu->C);
		break;
	case 0x0E:
		LD_r_n(cpu, &cpu->C);
		break;
	case 0x0F:
		RRCA(cpu);
		break;
	case 0x10:
		STOP(cpu);
		break;
	case 0x11:
		LD_rr_nn(cpu, &cpu->DE);
		break;
	case 0x12:
		LD_cDE_A(cpu);
		break;
	case 0x13:
		INC_rr(cpu, &cpu->DE);
		break;
	case 0x14:
		INC_r(cpu, &cpu->D);
		break;
	case 0x15:
		DEC_r(cpu, &cpu->D);
		break;
	case 0x16:
		LD_r_n(cpu, &cpu->D);
		break;
	case 0x17:
		RLA(cpu);
		break;
	case 0x18:
		JR_d(cpu);
		break;
	case 0x19:
		ADD_HL_rr(cpu, &cpu->DE);
		break;
	case 0x1A:
		LD_A_cDE(cpu);
		break;
	case 0x1B:
		DEC_rr(cpu, &cpu->DE);
		break;
	case 0x1C:
		INC_r(cpu, &cpu->E);
		break;
	case 0x1D:
		DEC_r(cpu, &cpu->E);
		break;
	case 0x1E:
		LD_r_n(cpu, &cpu->E);
		break;
	case 0x1F:
		RRA(cpu);
		break;
	case 0x20:
		JR_NZ_d(cpu);
		break;
	case 0x21:
		LD_rr_nn(cpu, &cpu->HL);
		break;
	case 0x22:
		LDI_cHL_A(cpu);
		break;
	case 0x23:
		INC_rr(cpu, &cpu->HL);
		break;
	case 0x24:
		INC_r(cpu, &cpu->H);
		break;
	case 0x25:
		DEC_r(cpu, &cpu->H);
		break;
	case 0x26:
		LD_r_n(cpu, &cpu->H);
		break;
	case 0x27:
		DAA(cpu);
		break;
	case 0x28:
		JR_Z_d(cpu);
		break;
	case 0x29:
		ADD_HL_rr(cpu, &cpu->HL);
		break;
	case 0x2A:
		LDI_A_cHL(cpu);
		break;
	case 0x2B:
		DEC_rr(cpu, &cpu->HL);
		break;
	case 0x2C:
		INC_r(cpu, &cpu->L);
		break;
	case 0x2D:
		DEC_r(cpu, &cpu->L);
		break;
	case 0x2E:
		LD_r_n(cpu, &cpu->L);
		break;
	case 0x2F:
		CPL(cpu);
		break;
	case 0x30:
		JR_NC_d(cpu);
		break;
	case 0x31:
		LD_rr_nn(cpu, &cpu->SP);
		break;
	case 0x32:
		LDD_cHL_A(cpu);
		break;
	case 0x33:
		INC_rr(cpu, &cpu->SP);
		break;
	case 0x34:
		INC_cHL(cpu);
		break;
	case 0x35:
		DEC_cHL(cpu);
		break;
	case 0x36:
		LD_cHL_n(cpu);
		break;
	case 0x37:
		SCF(cpu);
		break;
	case 0x38:
		JR_C_d(cpu);
		break;
	case 0x39:
		ADD_HL_rr(cpu, &cpu->SP);
		break;
	case 0x3A:
		LDD_A_cHL(cpu);
		break;
	case 0x3B:
		DEC_rr(cpu, &cpu->SP);
		break;
	case 0x3C:
		INC_r(cpu, &cpu->A);
		break;
	case 0x3D:
		DEC_r(cpu, &cpu->A);
		break;
	case 0x3E:
		LD_r_n(cpu, &cpu->A);
		break;
	case 0x3F:
		CCF(cpu);
		break;
	case 0x40:
		LD_r_r(cpu, &cpu->B, &cpu->B);
		break;
	case 0x41:
		LD_r_r(cpu, &cpu->B, &cpu->C);
		break;
	case 0x42:
		LD_r_r(cpu, &cpu->B, &cpu->D);
		break;
	case 0x43:
		LD_r_r(cpu, &cpu->B, &cpu->E);
		break;
	case 0x44:
		LD_r_r(cpu, &cpu->B, &cpu->H);
		break;
	case 0x45:
		LD_r_r(cpu, &cpu->B, &cpu->L);
		break;
	case 0x46:
		LD_r_cHL(cpu, &cpu->B);
		break;
	case 0x47:
		LD_r_r(cpu, &cpu->B, &cpu->A);
		break;
	case 0x48:
		LD_r_r(cpu, &cpu->C, &cpu->B);
		break;
	case 0x49:
		LD_r_r(cpu, &cpu->C, &cpu->C);
		break;
	case 0x4A:
		LD_r_r(cpu, &cpu->C, &cpu->D);
		break;
	case 0x4B:
		LD_r_r(cpu, &cpu->C, &cpu->E);
		break;
	case 0x4C:
		LD_r_r(cpu, &cpu->C, &cpu->H);
		break;
	case 0x4D:
		LD_r_r(cpu, &cpu->C, &cpu->L);
		break;
	case 0x4E:
		LD_r_cHL(cpu, &cpu->C);
		break;
	case 0x4F:
		LD_r_r(cpu, &cpu->C, &cpu->A);
		break;
	case 0x50:
		LD_r_r(cpu, &cpu->D, &cpu->B);
		break;
	case 0x51:
		LD_r_r(cpu, &cpu->D, &cpu->C);
		break;
	case 0x52:
		LD_r_r(cpu, &cpu->D, &cpu->D);
		break;
	case 0x53:
		LD_r_r(cpu, &cpu->D, &cpu->E);
		break;
	case 0x54:
		LD_r_r(cpu, &cpu->D, &cpu->H);
		break;
	case 0x55:
		LD_r_r(cpu, &cpu->D, &cpu->L);
		break;
	case 0x56:
		LD_r_cHL(cpu, &cpu->D);
		break;
	case 0x57:
		LD_r_r(cpu, &cpu->D, &cpu->A);
		break;
	case 0x58:
		LD_r_r(cpu, &cpu->E, &cpu->B);
		break;
	case 0x59:
		LD_r_r(cpu, &cpu->E, &cpu->C);
		break;
	case 0x5A:
		LD_r_r(cpu, &cpu->E, &cpu->D);
		break;
	case 0x5B:
		LD_r_r(cpu, &cpu->E, &cpu->E);
		break;
	case 0x5C:
		LD_r_r(cpu, &cpu->E, &cpu->H);
		break;
	case 0x5D:
		LD_r_r(cpu, &cpu->E, &cpu->L);
		break;
	case 0x5E:
		LD_r_cHL(cpu, &cpu->E);
		break;
	case 0x5F:
		LD_r_r(cpu, &cpu->E, &cpu->A);
		break;
	case 0x60:
		LD_r_r(cpu, &cpu->H, &cpu->B);
		break;
	case 0x61:
		LD_r_r(cpu, &cpu->H, &cpu->C);
		break;
	case 0x62:
		LD_r_r(cpu, &cpu->H, &cpu->D);
		break;
	case 0x63:
		LD_r_r(cpu, &cpu->H, &cpu->E);
		break;
	case 0x64:
		LD_r_r(cpu, &cpu->H, &cpu->H);
		break;
	case 0x65:
		LD_r_r(cpu, &cpu->H, &cpu->L);
		break;
	case 0x66:
		LD_r_cHL(cpu, &cpu->H);
		break;
	case 0x67:
		LD_r_r(cpu, &cpu->H, &cpu->A);
		break;
	case 0x68:
		LD_r_r(cpu, &cpu->L, &cpu->B);
		break;
	case 0x69:
		LD_r_r(cpu, &cpu->L, &cpu->C);
		break;
	case 0x6A:
		LD_r_r(cpu, &cpu->L, &cpu->D);
		break;
	case 0x6B:
		LD_r_r(cpu, &cpu->L, &cpu->E);
		break;
	case 0x6C:
		LD_r_r(cpu, &cpu->L, &cpu->H);
		break;
	case 0x6D:
		LD_r_r(cpu, &cpu->L, &cpu->L);
		break;
	case 0x6E:
		LD_r_cHL(cpu, &cpu->L);
		break;
	case 0x6F:
		LD_r_r(cpu, &cpu->L, &cpu->A);
		break;
	case 0x70:
		LD_cHL_r(cpu, &cpu->B);
		break;
	case 0x71:
		LD_cHL_r(cpu, &cpu->C);
		break;
	case 0x72:
		LD_cHL_r(cpu, &cpu->D);
		break;
	case 0x73:
		LD_cHL_r(cpu, &cpu->E);
		break;
	case 0x74:
		LD_cHL_r(cpu, &cpu->H);
		break;
	case 0x75:
		LD_cHL_r(cpu, &cpu->L);
		break;
	case 0x76:
		HALT(cpu);
		break;
	case 0x77:
		LD_cHL_r(cpu, &cpu->A);
		break;
	case 0x78:
		LD_r_r(cpu, &cpu->A, &cpu->B);
		break;
	case 0x79:
		LD_r_r(cpu, &cpu->A, &cpu->C);
		break;
	case 0x7A:
		LD_r_r(cpu, &cpu->A, &cpu->D);
		break;
	case 0x7B:
		LD_r_r(cpu, &cpu->A, &cpu->E);
		break;
	case 0x7C:
		LD_r_r(cpu, &cpu->A, &cpu->H);
		break;
	case 0x7D:
		LD_r_r(cpu, &cpu->A, &cpu->L);
		break;
	case 0x7E:
		LD_r_cHL(cpu, &cpu->A);
		break;
	case 0x7F:
		LD_r_r(cpu, &cpu->A, &cpu->A);
		break;
	case 0x80:
		ADD_A_r(cpu, &cpu->B);
		break;
	case 0x81:
		ADD_A_r(cpu, &cpu->C);
		break;
	case 0x82:
		ADD_A_r(cpu, &cpu->D);
		break;
	case 0x83:
		ADD_A_r(cpu, &cpu->E);
		break;
	case 0x84:
		ADD_A_r(cpu, &cpu->H);
		break;
	case 0x85:
		ADD_A_r(cpu, &cpu->L);
		break;
	case 0x86:
		ADD_A_cHL(cpu);
		break;
	case 0x87:
		ADD_A_r(cpu, &cpu->A);
		break;
	case 0x88:
		ADC_A_r(cpu, &cpu->B);
		break;
	case 0x89:
		ADC_A_r(cpu, &cpu->C);
		break;
	case 0x8A:
		ADC_A_r(cpu, &cpu->D);
		break;
	case 0x8B:
		ADC_A_r(cpu, &cpu->E);
		break;
	case 0x8C:
		ADC_A_r(cpu, &cpu->H);
		break;
	case 0x8D:
		ADC_A_r(cpu, &cpu->L);
		break;
	case 0x8E:
		ADC_A_cHL(cpu);
		break;
	case 0x8F:
		ADC_A_r(cpu, &cpu->A);
		break;
	case 0x90:
		SUB_A_r(cpu, &cpu->B);
		break;
	case 0x91:
		SUB_A_r(cpu, &cpu->C);
		break;
	case 0x92:
		SUB_A_r(cpu, &cpu->D);
		break;
	case 0x93:
		SUB_A_r(cpu, &cpu->E);
		break;
	case 0x94:
		SUB_A_r(cpu, &cpu->H);
		break;
	case 0x95:
		SUB_A_r(cpu, &cpu->L);
		break;
	case 0x96:
		SUB_A_cHL(cpu);
		break;
	case 0x97:
		SUB_A_r(cpu, &cpu->A);
		break;
	case 0x98:
		SBC_A_r(cpu, &cpu->B);
		break;
	case 0x99:
		SBC_A_r(cpu, &cpu->C);
		break;
	case 0x9A:
		SBC_A_r(cpu, &cpu->D);
		break;
	case 0x9B:
		SBC_A_r(cpu, &cpu->E);
		break;
	case 0x9C:
		SBC_A_r(cpu, &cpu->H);
		break;
	case 0x9D:
		SBC_A_r(cpu, &cpu->L);
		break;
	case 0x9E:
		SBC_A_cHL(cpu);
		break;
	case 0x9F:
		SBC_A_r(cpu, &cpu->A);
		break;
	case 0xA0:
		AND_r(cpu, &cpu->B);
		break;
	case 0xA1:
		AND_r(cpu, &cpu->C);
		break;
	case 0xA2:
		AND_r(cpu, &cpu->D);
		break;
	case 0xA3:
		AND_r(cpu, &cpu->E);
		break;
	case 0xA4:
		AND_r(cpu, &cpu->H);
		break;
	case 0xA5:
		AND_r(cpu, &cpu->L);
		break;
	case 0xA6:
		AND_cHL(cpu);
		break;
	case 0xA7:
		AND_r(cpu, &cpu->A);
		break;
	case 0xA8:
		XOR_r(cpu, &cpu->B);
		break;
	case 0xA9:
		XOR_r(cpu, &cpu->C);
		break;
	case 0xAA:
		XOR_r(cpu, &cpu->D);
		break;
	case 0xAB:
		XOR_r(cpu, &cpu->E);
		break;
	case 0xAC:
		XOR_r(cpu, &cpu->H);
		break;
	case 0xAD:
		XOR_r(cpu, &cpu->L);
		break;
	case 0xAE:
		XOR_cHL(cpu);
		break;
	case 0xAF:
		XOR_r(cpu, &cpu->A);
		break;
	case 0xB0:
		OR_r(cpu, &cpu->B);
		break;
	case 0xB1:
		OR_r(cpu, &cpu->C);
		break;
	case 0xB2:
		OR_r(cpu, &cpu->D);
		break;
	case 0xB3:
		OR_r(cpu, &cpu->E);
		break;
	case 0xB4:
		OR_r(cpu, &cpu->H);
		break;
	case 0xB5:
		OR_r(cpu, &cpu->L);
		break;
	case 0xB6:
		OR_cHL(cpu);
		break;
	case 0xB7:
		OR_r(cpu, &cpu->A);
		break;
	case 0xB8:
		CP_r(cpu, &cpu->B);
		break;
	case 0xB9:
		CP_r(cpu, &cpu->C);
		break;
	case 0xBA:
		CP_r(cpu, &cpu->D);
		break;
	case 0xBB:
		CP_r(cpu, &cpu->E);
		break;
	case 0xBC:
		CP_r(cpu, &cpu->H);
		break;
	case 0xBD:
		CP_r(cpu, &cpu->L);
		break;
	case 0xBE:
		CP_cHL(cpu);
		break;
	case 0xBF:
		CP_r(cpu, &cpu->A);
		break;
	case 0xC0:
		RET_NZ(cpu);
		break;
	case 0xC1:
		POP_rr(cpu, &cpu->BC);
		break;
	case 0xC2:
		JP_NZ_nn(cpu);
		break;
	case 0xC3:
		JP_nn(cpu);
		break;
	case 0xC4:
		CALL_NZ_nn(cpu);
		break;
	case 0xC5:
		PUSH_rr(cpu, &cpu->BC);
		break;
	case 0xC6:
		ADD_A_n(cpu);
		break;
	case 0xC7:
		RST_n(cpu, 0x00);
		break;
	case 0xC8:
		RET_Z(cpu);
		break;
	case 0xC9:
		RET(cpu);
		break;
	case 0xCA:
		JP_Z_nn(cpu);
		break;
	case 0xCB:
		lr35902_opcode_CB(cpu);
		break;
	case 0xCC:
		CALL_Z_nn(cpu);
		break;
	case 0xCD:
		CALL_nn(cpu);
		break;
	case 0xCE:
		ADC_A_n(cpu);
		break;
	case 0xCF:
		RST_n(cpu, 0x08);
		break;
	case 0xD0:
		RET_NC(cpu);
		break;
	case 0xD1:
		POP_rr(cpu, &cpu->DE);
		break;
	case 0xD2:
		JP_NC_nn(cpu);
		break;
	case 0xD4:
		CALL_NC_nn(cpu);
		break;
	case 0xD5:
		PUSH_rr(cpu, &cpu->DE);
		break;
	case 0xD6:
		SUB_A_n(cpu);
		break;
	case 0xD7:
		RST_n(cpu, 0x10);
		break;
	case 0xD8:
		RET_C(cpu);
		break;
	case 0xD9:
		RETI(cpu);
		break;
	case 0xDA:
		JP_C_nn(cpu);
		break;
	case 0xDC:
		CALL_C_nn(cpu);
		break;
	case 0xDE:
		SBC_A_n(cpu);
		break;
	case 0xDF:
		RST_n(cpu, 0x18);
		break;
	case 0xE0:
		LD_cFF00pn_A(cpu);
		break;
	case 0xE1:
		POP_rr(cpu, &cpu->HL);
		break;
	case 0xE2:
		LD_cFF00pC_A(cpu);
		break;
	case 0xE5:
		PUSH_rr(cpu, &cpu->HL);
		break;
	case 0xE6:
		AND_n(cpu);
		break;
	case 0xE7:
		RST_n(cpu, 0x20);
		break;
	case 0xE8:
		ADD_SP_d(cpu);
		break;
	case 0xE9:
		JP_HL(cpu);
		break;
	case 0xEA:
		LD_cnn_A(cpu);
		break;
	case 0xEE:
		XOR_n(cpu);
		break;
	case 0xEF:
		RST_n(cpu, 0x28);
		break;
	case 0xF0:
		LD_A_cFF00pn(cpu);
		break;
	case 0xF1:
		POP_AF(cpu);
		break;
	case 0xF2:
		LD_A_cFF00pC(cpu);
		break;
	case 0xF3:
		DI(cpu);
		break;
	case 0xF5:
		PUSH_rr(cpu, &cpu->AF);
		break;
	case 0xF6:
		OR_n(cpu);
		break;
	case 0xF7:
		RST_n(cpu, 0x30);
		break;
	case 0xF8:
		LD_HL_SPpd(cpu);
		break;
	case 0xF9:
		LD_SP_HL(cpu);
		break;
	case 0xFA:
		LD_A_cnn(cpu);
		break;
	case 0xFB:
		EI(cpu);
		break;
	case 0xFE:
		CP_n(cpu);
		break;
	case 0xFF:
		RST_n(cpu, 0x38);
		break;
	default:
		LOG_W("lr35902: unknown opcode (%02x)!\n", opcode);
		clock_consume(1);
		break;
	}
}

void lr35902_opcode_CB(struct lr35902 *cpu)
{
	uint8_t opcode;

	/* Fetch CB opcode */
	opcode = memory_readb(cpu->bus_id, cpu->PC++);

	/* Execute CB opcode */
	switch (opcode) {
	case 0x00:
		RLC_r(cpu, &cpu->B);
		break;
	case 0x01:
		RLC_r(cpu, &cpu->C);
		break;
	case 0x02:
		RLC_r(cpu, &cpu->D);
		break;
	case 0x03:
		RLC_r(cpu, &cpu->E);
		break;
	case 0x04:
		RLC_r(cpu, &cpu->H);
		break;
	case 0x05:
		RLC_r(cpu, &cpu->L);
		break;
	case 0x06:
		RLC_cHL(cpu);
		break;
	case 0x07:
		RLC_r(cpu, &cpu->A);
		break;
	case 0x08:
		RRC_r(cpu, &cpu->B);
		break;
	case 0x09:
		RRC_r(cpu, &cpu->C);
		break;
	case 0x0A:
		RRC_r(cpu, &cpu->D);
		break;
	case 0x0B:
		RRC_r(cpu, &cpu->E);
		break;
	case 0x0C:
		RRC_r(cpu, &cpu->H);
		break;
	case 0x0D:
		RRC_r(cpu, &cpu->L);
		break;
	case 0x0E:
		RRC_cHL(cpu);
		break;
	case 0x0F:
		RRC_r(cpu, &cpu->A);
		break;
	case 0x10:
		RL_r(cpu, &cpu->B);
		break;
	case 0x11:
		RL_r(cpu, &cpu->C);
		break;
	case 0x12:
		RL_r(cpu, &cpu->D);
		break;
	case 0x13:
		RL_r(cpu, &cpu->E);
		break;
	case 0x14:
		RL_r(cpu, &cpu->H);
		break;
	case 0x15:
		RL_r(cpu, &cpu->L);
		break;
	case 0x16:
		RL_cHL(cpu);
		break;
	case 0x17:
		RL_r(cpu, &cpu->A);
		break;
	case 0x18:
		RR_r(cpu, &cpu->B);
		break;
	case 0x19:
		RR_r(cpu, &cpu->C);
		break;
	case 0x1A:
		RR_r(cpu, &cpu->D);
		break;
	case 0x1B:
		RR_r(cpu, &cpu->E);
		break;
	case 0x1C:
		RR_r(cpu, &cpu->H);
		break;
	case 0x1D:
		RR_r(cpu, &cpu->L);
		break;
	case 0x1E:
		RR_cHL(cpu);
		break;
	case 0x1F:
		RR_r(cpu, &cpu->A);
		break;
	case 0x20:
		SLA_r(cpu, &cpu->B);
		break;
	case 0x21:
		SLA_r(cpu, &cpu->C);
		break;
	case 0x22:
		SLA_r(cpu, &cpu->D);
		break;
	case 0x23:
		SLA_r(cpu, &cpu->E);
		break;
	case 0x24:
		SLA_r(cpu, &cpu->H);
		break;
	case 0x25:
		SLA_r(cpu, &cpu->L);
		break;
	case 0x26:
		SLA_cHL(cpu);
		break;
	case 0x27:
		SLA_r(cpu, &cpu->A);
		break;
	case 0x28:
		SRA_r(cpu, &cpu->B);
		break;
	case 0x29:
		SRA_r(cpu, &cpu->C);
		break;
	case 0x2A:
		SRA_r(cpu, &cpu->D);
		break;
	case 0x2B:
		SRA_r(cpu, &cpu->E);
		break;
	case 0x2C:
		SRA_r(cpu, &cpu->H);
		break;
	case 0x2D:
		SRA_r(cpu, &cpu->L);
		break;
	case 0x2E:
		SRA_cHL(cpu);
		break;
	case 0x2F:
		SRA_r(cpu, &cpu->A);
		break;
	case 0x30:
		SWAP_r(cpu, &cpu->B);
		break;
	case 0x31:
		SWAP_r(cpu, &cpu->C);
		break;
	case 0x32:
		SWAP_r(cpu, &cpu->D);
		break;
	case 0x33:
		SWAP_r(cpu, &cpu->E);
		break;
	case 0x34:
		SWAP_r(cpu, &cpu->H);
		break;
	case 0x35:
		SWAP_r(cpu, &cpu->L);
		break;
	case 0x36:
		SWAP_cHL(cpu);
		break;
	case 0x37:
		SWAP_r(cpu, &cpu->A);
		break;
	case 0x38:
		SRL_r(cpu, &cpu->B);
		break;
	case 0x39:
		SRL_r(cpu, &cpu->C);
		break;
	case 0x3A:
		SRL_r(cpu, &cpu->D);
		break;
	case 0x3B:
		SRL_r(cpu, &cpu->E);
		break;
	case 0x3C:
		SRL_r(cpu, &cpu->H);
		break;
	case 0x3D:
		SRL_r(cpu, &cpu->L);
		break;
	case 0x3E:
		SRL_cHL(cpu);
		break;
	case 0x3F:
		SRL_r(cpu, &cpu->A);
		break;
	case 0x40:
		BIT_n_r(cpu, 0, &cpu->B);
		break;
	case 0x41:
		BIT_n_r(cpu, 0, &cpu->C);
		break;
	case 0x42:
		BIT_n_r(cpu, 0, &cpu->D);
		break;
	case 0x43:
		BIT_n_r(cpu, 0, &cpu->E);
		break;
	case 0x44:
		BIT_n_r(cpu, 0, &cpu->H);
		break;
	case 0x45:
		BIT_n_r(cpu, 0, &cpu->L);
		break;
	case 0x46:
		BIT_n_cHL(cpu, 0);
		break;
	case 0x47:
		BIT_n_r(cpu, 0, &cpu->A);
		break;
	case 0x48:
		BIT_n_r(cpu, 1, &cpu->B);
		break;
	case 0x49:
		BIT_n_r(cpu, 1, &cpu->C);
		break;
	case 0x4A:
		BIT_n_r(cpu, 1, &cpu->D);
		break;
	case 0x4B:
		BIT_n_r(cpu, 1, &cpu->E);
		break;
	case 0x4C:
		BIT_n_r(cpu, 1, &cpu->H);
		break;
	case 0x4D:
		BIT_n_r(cpu, 1, &cpu->L);
		break;
	case 0x4E:
		BIT_n_cHL(cpu, 1);
		break;
	case 0x4F:
		BIT_n_r(cpu, 1, &cpu->A);
		break;
	case 0x50:
		BIT_n_r(cpu, 2, &cpu->B);
		break;
	case 0x51:
		BIT_n_r(cpu, 2, &cpu->C);
		break;
	case 0x52:
		BIT_n_r(cpu, 2, &cpu->D);
		break;
	case 0x53:
		BIT_n_r(cpu, 2, &cpu->E);
		break;
	case 0x54:
		BIT_n_r(cpu, 2, &cpu->H);
		break;
	case 0x55:
		BIT_n_r(cpu, 2, &cpu->L);
		break;
	case 0x56:
		BIT_n_cHL(cpu, 2);
		break;
	case 0x57:
		BIT_n_r(cpu, 2, &cpu->A);
		break;
	case 0x58:
		BIT_n_r(cpu, 3, &cpu->B);
		break;
	case 0x59:
		BIT_n_r(cpu, 3, &cpu->C);
		break;
	case 0x5A:
		BIT_n_r(cpu, 3, &cpu->D);
		break;
	case 0x5B:
		BIT_n_r(cpu, 3, &cpu->E);
		break;
	case 0x5C:
		BIT_n_r(cpu, 3, &cpu->H);
		break;
	case 0x5D:
		BIT_n_r(cpu, 3, &cpu->L);
		break;
	case 0x5E:
		BIT_n_cHL(cpu, 3);
		break;
	case 0x5F:
		BIT_n_r(cpu, 3, &cpu->A);
		break;
	case 0x60:
		BIT_n_r(cpu, 4, &cpu->B);
		break;
	case 0x61:
		BIT_n_r(cpu, 4, &cpu->C);
		break;
	case 0x62:
		BIT_n_r(cpu, 4, &cpu->D);
		break;
	case 0x63:
		BIT_n_r(cpu, 4, &cpu->E);
		break;
	case 0x64:
		BIT_n_r(cpu, 4, &cpu->H);
		break;
	case 0x65:
		BIT_n_r(cpu, 4, &cpu->L);
		break;
	case 0x66:
		BIT_n_cHL(cpu, 4);
		break;
	case 0x67:
		BIT_n_r(cpu, 4, &cpu->A);
		break;
	case 0x68:
		BIT_n_r(cpu, 5, &cpu->B);
		break;
	case 0x69:
		BIT_n_r(cpu, 5, &cpu->C);
		break;
	case 0x6A:
		BIT_n_r(cpu, 5, &cpu->D);
		break;
	case 0x6B:
		BIT_n_r(cpu, 5, &cpu->E);
		break;
	case 0x6C:
		BIT_n_r(cpu, 5, &cpu->H);
		break;
	case 0x6D:
		BIT_n_r(cpu, 5, &cpu->L);
		break;
	case 0x6E:
		BIT_n_cHL(cpu, 5);
		break;
	case 0x6F:
		BIT_n_r(cpu, 5, &cpu->A);
		break;
	case 0x70:
		BIT_n_r(cpu, 6, &cpu->B);
		break;
	case 0x71:
		BIT_n_r(cpu, 6, &cpu->C);
		break;
	case 0x72:
		BIT_n_r(cpu, 6, &cpu->D);
		break;
	case 0x73:
		BIT_n_r(cpu, 6, &cpu->E);
		break;
	case 0x74:
		BIT_n_r(cpu, 6, &cpu->H);
		break;
	case 0x75:
		BIT_n_r(cpu, 6, &cpu->L);
		break;
	case 0x76:
		BIT_n_cHL(cpu, 6);
		break;
	case 0x77:
		BIT_n_r(cpu, 6, &cpu->A);
		break;
	case 0x78:
		BIT_n_r(cpu, 7, &cpu->B);
		break;
	case 0x79:
		BIT_n_r(cpu, 7, &cpu->C);
		break;
	case 0x7A:
		BIT_n_r(cpu, 7, &cpu->D);
		break;
	case 0x7B:
		BIT_n_r(cpu, 7, &cpu->E);
		break;
	case 0x7C:
		BIT_n_r(cpu, 7, &cpu->H);
		break;
	case 0x7D:
		BIT_n_r(cpu, 7, &cpu->L);
		break;
	case 0x7E:
		BIT_n_cHL(cpu, 7);
		break;
	case 0x7F:
		BIT_n_r(cpu, 7, &cpu->A);
		break;
	case 0x80:
		RES_n_r(cpu, 0, &cpu->B);
		break;
	case 0x81:
		RES_n_r(cpu, 0, &cpu->C);
		break;
	case 0x82:
		RES_n_r(cpu, 0, &cpu->D);
		break;
	case 0x83:
		RES_n_r(cpu, 0, &cpu->E);
		break;
	case 0x84:
		RES_n_r(cpu, 0, &cpu->H);
		break;
	case 0x85:
		RES_n_r(cpu, 0, &cpu->L);
		break;
	case 0x86:
		RES_n_cHL(cpu, 0);
		break;
	case 0x87:
		RES_n_r(cpu, 0, &cpu->A);
		break;
	case 0x88:
		RES_n_r(cpu, 1, &cpu->B);
		break;
	case 0x89:
		RES_n_r(cpu, 1, &cpu->C);
		break;
	case 0x8A:
		RES_n_r(cpu, 1, &cpu->D);
		break;
	case 0x8B:
		RES_n_r(cpu, 1, &cpu->E);
		break;
	case 0x8C:
		RES_n_r(cpu, 1, &cpu->H);
		break;
	case 0x8D:
		RES_n_r(cpu, 1, &cpu->L);
		break;
	case 0x8E:
		RES_n_cHL(cpu, 1);
		break;
	case 0x8F:
		RES_n_r(cpu, 1, &cpu->A);
		break;
	case 0x90:
		RES_n_r(cpu, 2, &cpu->B);
		break;
	case 0x91:
		RES_n_r(cpu, 2, &cpu->C);
		break;
	case 0x92:
		RES_n_r(cpu, 2, &cpu->D);
		break;
	case 0x93:
		RES_n_r(cpu, 2, &cpu->E);
		break;
	case 0x94:
		RES_n_r(cpu, 2, &cpu->H);
		break;
	case 0x95:
		RES_n_r(cpu, 2, &cpu->L);
		break;
	case 0x96:
		RES_n_cHL(cpu, 2);
		break;
	case 0x97:
		RES_n_r(cpu, 2, &cpu->A);
		break;
	case 0x98:
		RES_n_r(cpu, 3, &cpu->B);
		break;
	case 0x99:
		RES_n_r(cpu, 3, &cpu->C);
		break;
	case 0x9A:
		RES_n_r(cpu, 3, &cpu->D);
		break;
	case 0x9B:
		RES_n_r(cpu, 3, &cpu->E);
		break;
	case 0x9C:
		RES_n_r(cpu, 3, &cpu->H);
		break;
	case 0x9D:
		RES_n_r(cpu, 3, &cpu->L);
		break;
	case 0x9E:
		RES_n_cHL(cpu, 3);
		break;
	case 0x9F:
		RES_n_r(cpu, 3, &cpu->A);
		break;
	case 0xA0:
		RES_n_r(cpu, 4, &cpu->B);
		break;
	case 0xA1:
		RES_n_r(cpu, 4, &cpu->C);
		break;
	case 0xA2:
		RES_n_r(cpu, 4, &cpu->D);
		break;
	case 0xA3:
		RES_n_r(cpu, 4, &cpu->E);
		break;
	case 0xA4:
		RES_n_r(cpu, 4, &cpu->H);
		break;
	case 0xA5:
		RES_n_r(cpu, 4, &cpu->L);
		break;
	case 0xA6:
		RES_n_cHL(cpu, 4);
		break;
	case 0xA7:
		RES_n_r(cpu, 4, &cpu->A);
		break;
	case 0xA8:
		RES_n_r(cpu, 5, &cpu->B);
		break;
	case 0xA9:
		RES_n_r(cpu, 5, &cpu->C);
		break;
	case 0xAA:
		RES_n_r(cpu, 5, &cpu->D);
		break;
	case 0xAB:
		RES_n_r(cpu, 5, &cpu->E);
		break;
	case 0xAC:
		RES_n_r(cpu, 5, &cpu->H);
		break;
	case 0xAD:
		RES_n_r(cpu, 5, &cpu->L);
		break;
	case 0xAE:
		RES_n_cHL(cpu, 5);
		break;
	case 0xAF:
		RES_n_r(cpu, 5, &cpu->A);
		break;
	case 0xB0:
		RES_n_r(cpu, 6, &cpu->B);
		break;
	case 0xB1:
		RES_n_r(cpu, 6, &cpu->C);
		break;
	case 0xB2:
		RES_n_r(cpu, 6, &cpu->D);
		break;
	case 0xB3:
		RES_n_r(cpu, 6, &cpu->E);
		break;
	case 0xB4:
		RES_n_r(cpu, 6, &cpu->H);
		break;
	case 0xB5:
		RES_n_r(cpu, 6, &cpu->L);
		break;
	case 0xB6:
		RES_n_cHL(cpu, 6);
		break;
	case 0xB7:
		RES_n_r(cpu, 6, &cpu->A);
		break;
	case 0xB8:
		RES_n_r(cpu, 7, &cpu->B);
		break;
	case 0xB9:
		RES_n_r(cpu, 7, &cpu->C);
		break;
	case 0xBA:
		RES_n_r(cpu, 7, &cpu->D);
		break;
	case 0xBB:
		RES_n_r(cpu, 7, &cpu->E);
		break;
	case 0xBC:
		RES_n_r(cpu, 7, &cpu->H);
		break;
	case 0xBD:
		RES_n_r(cpu, 7, &cpu->L);
		break;
	case 0xBE:
		RES_n_cHL(cpu, 7);
		break;
	case 0xBF:
		RES_n_r(cpu, 7, &cpu->A);
		break;
	case 0xC0:
		SET_n_r(cpu, 0, &cpu->B);
		break;
	case 0xC1:
		SET_n_r(cpu, 0, &cpu->C);
		break;
	case 0xC2:
		SET_n_r(cpu, 0, &cpu->D);
		break;
	case 0xC3:
		SET_n_r(cpu, 0, &cpu->E);
		break;
	case 0xC4:
		SET_n_r(cpu, 0, &cpu->H);
		break;
	case 0xC5:
		SET_n_r(cpu, 0, &cpu->L);
		break;
	case 0xC6:
		SET_n_cHL(cpu, 0);
		break;
	case 0xC7:
		SET_n_r(cpu, 0, &cpu->A);
		break;
	case 0xC8:
		SET_n_r(cpu, 1, &cpu->B);
		break;
	case 0xC9:
		SET_n_r(cpu, 1, &cpu->C);
		break;
	case 0xCA:
		SET_n_r(cpu, 1, &cpu->D);
		break;
	case 0xCB:
		SET_n_r(cpu, 1, &cpu->E);
		break;
	case 0xCC:
		SET_n_r(cpu, 1, &cpu->H);
		break;
	case 0xCD:
		SET_n_r(cpu, 1, &cpu->L);
		break;
	case 0xCE:
		SET_n_cHL(cpu, 1);
		break;
	case 0xCF:
		SET_n_r(cpu, 1, &cpu->A);
		break;
	case 0xD0:
		SET_n_r(cpu, 2, &cpu->B);
		break;
	case 0xD1:
		SET_n_r(cpu, 2, &cpu->C);
		break;
	case 0xD2:
		SET_n_r(cpu, 2, &cpu->D);
		break;
	case 0xD3:
		SET_n_r(cpu, 2, &cpu->E);
		break;
	case 0xD4:
		SET_n_r(cpu, 2, &cpu->H);
		break;
	case 0xD5:
		SET_n_r(cpu, 2, &cpu->L);
		break;
	case 0xD6:
		SET_n_cHL(cpu, 2);
		break;
	case 0xD7:
		SET_n_r(cpu, 2, &cpu->A);
		break;
	case 0xD8:
		SET_n_r(cpu, 3, &cpu->B);
		break;
	case 0xD9:
		SET_n_r(cpu, 3, &cpu->C);
		break;
	case 0xDA:
		SET_n_r(cpu, 3, &cpu->D);
		break;
	case 0xDB:
		SET_n_r(cpu, 3, &cpu->E);
		break;
	case 0xDC:
		SET_n_r(cpu, 3, &cpu->H);
		break;
	case 0xDD:
		SET_n_r(cpu, 3, &cpu->L);
		break;
	case 0xDE:
		SET_n_cHL(cpu, 3);
		break;
	case 0xDF:
		SET_n_r(cpu, 3, &cpu->A);
		break;
	case 0xE0:
		SET_n_r(cpu, 4, &cpu->B);
		break;
	case 0xE1:
		SET_n_r(cpu, 4, &cpu->C);
		break;
	case 0xE2:
		SET_n_r(cpu, 4, &cpu->D);
		break;
	case 0xE3:
		SET_n_r(cpu, 4, &cpu->E);
		break;
	case 0xE4:
		SET_n_r(cpu, 4, &cpu->H);
		break;
	case 0xE5:
		SET_n_r(cpu, 4, &cpu->L);
		break;
	case 0xE6:
		SET_n_cHL(cpu, 4);
		break;
	case 0xE7:
		SET_n_r(cpu, 4, &cpu->A);
		break;
	case 0xE8:
		SET_n_r(cpu, 5, &cpu->B);
		break;
	case 0xE9:
		SET_n_r(cpu, 5, &cpu->C);
		break;
	case 0xEA:
		SET_n_r(cpu, 5, &cpu->D);
		break;
	case 0xEB:
		SET_n_r(cpu, 5, &cpu->E);
		break;
	case 0xEC:
		SET_n_r(cpu, 5, &cpu->H);
		break;
	case 0xED:
		SET_n_r(cpu, 5, &cpu->L);
		break;
	case 0xEE:
		SET_n_cHL(cpu, 5);
		break;
	case 0xEF:
		SET_n_r(cpu, 5, &cpu->A);
		break;
	case 0xF0:
		SET_n_r(cpu, 6, &cpu->B);
		break;
	case 0xF1:
		SET_n_r(cpu, 6, &cpu->C);
		break;
	case 0xF2:
		SET_n_r(cpu, 6, &cpu->D);
		break;
	case 0xF3:
		SET_n_r(cpu, 6, &cpu->E);
		break;
	case 0xF4:
		SET_n_r(cpu, 6, &cpu->H);
		break;
	case 0xF5:
		SET_n_r(cpu, 6, &cpu->L);
		break;
	case 0xF6:
		SET_n_cHL(cpu, 6);
		break;
	case 0xF7:
		SET_n_r(cpu, 6, &cpu->A);
		break;
	case 0xF8:
		SET_n_r(cpu, 7, &cpu->B);
		break;
	case 0xF9:
		SET_n_r(cpu, 7, &cpu->C);
		break;
	case 0xFA:
		SET_n_r(cpu, 7, &cpu->D);
		break;
	case 0xFB:
		SET_n_r(cpu, 7, &cpu->E);
		break;
	case 0xFC:
		SET_n_r(cpu, 7, &cpu->H);
		break;
	case 0xFD:
		SET_n_r(cpu, 7, &cpu->L);
		break;
	case 0xFE:
		SET_n_cHL(cpu, 7);
		break;
	case 0xFF:
		SET_n_r(cpu, 7, &cpu->A);
		break;
	default:
		LOG_W("lr35902: unknown opcode (%02x)!\n", opcode);
		clock_consume(1);
		break;
	}
}

bool lr35902_init(struct cpu_instance *instance)
{
	struct lr35902 *cpu;
	struct resource *res;

	/* Allocate lr35902 structure and set private data */
	cpu = calloc(1, sizeof(struct lr35902));
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
	cpu->clock.tick = (clock_tick_t)lr35902_tick;
	clock_add(&cpu->clock);

	/* Add IF memory region */
	res = resource_get("ifr",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	cpu->if_region.area = res;
	cpu->if_region.mops = &ram_mops;
	cpu->if_region.data = &cpu->IF;
	memory_region_add(&cpu->if_region);

	/* Add IE memory region */
	res = resource_get("ier",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	cpu->ie_region.area = res;
	cpu->ie_region.mops = &ram_mops;
	cpu->ie_region.data = &cpu->IE;
	memory_region_add(&cpu->ie_region);

	return true;
}

void lr35902_reset(struct cpu_instance *instance)
{
	struct lr35902 *cpu = instance->priv_data;

	/* Initialize processor data */
	cpu->PC = 0;
	cpu->IME = 0;
	cpu->IF = 0;
	cpu->IE = 0;

	/* Enable clock */
	cpu->clock.enabled = true;
}

void lr35902_interrupt(struct cpu_instance *instance, int irq)
{
	struct lr35902 *cpu = instance->priv_data;

	/* Flag interrupt request in IF register */
	cpu->IF |= BIT(irq);
}

void lr35902_deinit(struct cpu_instance *instance)
{
	struct lr35902 *cpu = instance->priv_data;
	free(cpu);
}

CPU_START(lr35902)
	.init = lr35902_init,
	.reset = lr35902_reset,
	.interrupt = lr35902_interrupt,
	.deinit = lr35902_deinit
CPU_END

