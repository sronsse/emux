#include <stdio.h>
#include <stdlib.h>
#include <clock.h>
#include <cpu.h>
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
	bool halted;
	int bus_id;
	struct clock clock;
};

static bool lr35902_init(struct cpu_instance *instance);
static void lr35902_interrupt(struct cpu_instance *instance, int irq);
static void lr35902_deinit(struct cpu_instance *instance);
static void lr35902_tick(clock_data_t *data);
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
	uint8_t old_A = cpu->A;
	uint8_t correction_factor;
	if ((cpu->A > 0x99) || cpu->flags.C) {
		correction_factor = 0x60;
		cpu->flags.C = 1;
	} else {
		correction_factor = 0x00;
		cpu->flags.C = 0;
	}
	if (((cpu->A & 0x0F) > 0x09) || cpu->flags.H)
		correction_factor |= 0x06;
	cpu->A += cpu->flags.N ? -correction_factor : correction_factor;
	cpu->flags.H = (old_A ^ cpu->A) >> 4;
	cpu->flags.Z = (cpu->A == 0);
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
	cpu->halted = 1;
	clock_consume(4);
}

void STOP(struct lr35902 *cpu)
{
	memory_readb(cpu->bus_id, cpu->PC++);
	cpu->halted = 1;
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

void lr35902_tick(clock_data_t *data)
{
	struct lr35902 *cpu = data;
	uint8_t opcode;

	/* Fetch opcode */
	opcode = memory_readb(cpu->bus_id, cpu->PC++);

	/* Execute opcode */
	switch (opcode) {
	case 0x00:
		return NOP(cpu);
	case 0x01:
		return LD_rr_nn(cpu, &cpu->BC);
	case 0x02:
		return LD_cBC_A(cpu);
	case 0x03:
		return INC_rr(cpu, &cpu->BC);
	case 0x04:
		return INC_r(cpu, &cpu->B);
	case 0x05:
		return DEC_r(cpu, &cpu->B);
	case 0x06:
		return LD_r_n(cpu, &cpu->B);
	case 0x07:
		return RLCA(cpu);
	case 0x08:
		return LD_cnn_SP(cpu);
	case 0x09:
		return ADD_HL_rr(cpu, &cpu->BC);
	case 0x0A:
		return LD_A_cBC(cpu);
	case 0x0B:
		return DEC_rr(cpu, &cpu->BC);
	case 0x0C:
		return INC_r(cpu, &cpu->C);
	case 0x0D:
		return DEC_r(cpu, &cpu->C);
	case 0x0E:
		return LD_r_n(cpu, &cpu->C);
	case 0x0F:
		return RRCA(cpu);
	case 0x10:
		return STOP(cpu);
	case 0x11:
		return LD_rr_nn(cpu, &cpu->DE);
	case 0x12:
		return LD_cDE_A(cpu);
	case 0x13:
		return INC_rr(cpu, &cpu->DE);
	case 0x14:
		return INC_r(cpu, &cpu->D);
	case 0x15:
		return DEC_r(cpu, &cpu->D);
	case 0x16:
		return LD_r_n(cpu, &cpu->D);
	case 0x17:
		return RLA(cpu);
	case 0x18:
		return JR_d(cpu);
	case 0x19:
		return ADD_HL_rr(cpu, &cpu->DE);
	case 0x1A:
		return LD_A_cDE(cpu);
	case 0x1B:
		return DEC_rr(cpu, &cpu->DE);
	case 0x1C:
		return INC_r(cpu, &cpu->E);
	case 0x1D:
		return DEC_r(cpu, &cpu->E);
	case 0x1E:
		return LD_r_n(cpu, &cpu->E);
	case 0x1F:
		return RRA(cpu);
	case 0x20:
		return JR_NZ_d(cpu);
	case 0x21:
		return LD_rr_nn(cpu, &cpu->HL);
	case 0x22:
		return LDI_cHL_A(cpu);
	case 0x23:
		return INC_rr(cpu, &cpu->HL);
	case 0x24:
		return INC_r(cpu, &cpu->H);
	case 0x25:
		return DEC_r(cpu, &cpu->H);
	case 0x26:
		return LD_r_n(cpu, &cpu->H);
	case 0x27:
		return DAA(cpu);
	case 0x28:
		return JR_Z_d(cpu);
	case 0x29:
		return ADD_HL_rr(cpu, &cpu->HL);
	case 0x2A:
		return LDI_A_cHL(cpu);
	case 0x2B:
		return DEC_rr(cpu, &cpu->HL);
	case 0x2C:
		return INC_r(cpu, &cpu->L);
	case 0x2D:
		return DEC_r(cpu, &cpu->L);
	case 0x2E:
		return LD_r_n(cpu, &cpu->L);
	case 0x2F:
		return CPL(cpu);
	case 0x30:
		return JR_NC_d(cpu);
	case 0x31:
		return LD_rr_nn(cpu, &cpu->SP);
	case 0x32:
		return LDD_cHL_A(cpu);
	case 0x33:
		return INC_rr(cpu, &cpu->SP);
	case 0x34:
		return INC_cHL(cpu);
	case 0x35:
		return DEC_cHL(cpu);
	case 0x36:
		return LD_cHL_n(cpu);
	case 0x37:
		return SCF(cpu);
	case 0x38:
		return JR_C_d(cpu);
	case 0x39:
		return ADD_HL_rr(cpu, &cpu->SP);
	case 0x3A:
		return LDD_A_cHL(cpu);
	case 0x3B:
		return DEC_rr(cpu, &cpu->SP);
	case 0x3C:
		return INC_r(cpu, &cpu->A);
	case 0x3D:
		return DEC_r(cpu, &cpu->A);
	case 0x3E:
		return LD_r_n(cpu, &cpu->A);
	case 0x3F:
		return CCF(cpu);
	case 0x40:
		return LD_r_r(cpu, &cpu->B, &cpu->B);
	case 0x41:
		return LD_r_r(cpu, &cpu->B, &cpu->C);
	case 0x42:
		return LD_r_r(cpu, &cpu->B, &cpu->D);
	case 0x43:
		return LD_r_r(cpu, &cpu->B, &cpu->E);
	case 0x44:
		return LD_r_r(cpu, &cpu->B, &cpu->H);
	case 0x45:
		return LD_r_r(cpu, &cpu->B, &cpu->L);
	case 0x46:
		return LD_r_cHL(cpu, &cpu->B);
	case 0x47:
		return LD_r_r(cpu, &cpu->B, &cpu->A);
	case 0x48:
		return LD_r_r(cpu, &cpu->C, &cpu->B);
	case 0x49:
		return LD_r_r(cpu, &cpu->C, &cpu->C);
	case 0x4A:
		return LD_r_r(cpu, &cpu->C, &cpu->D);
	case 0x4B:
		return LD_r_r(cpu, &cpu->C, &cpu->E);
	case 0x4C:
		return LD_r_r(cpu, &cpu->C, &cpu->H);
	case 0x4D:
		return LD_r_r(cpu, &cpu->C, &cpu->L);
	case 0x4E:
		return LD_r_cHL(cpu, &cpu->C);
	case 0x4F:
		return LD_r_r(cpu, &cpu->C, &cpu->A);
	case 0x50:
		return LD_r_r(cpu, &cpu->D, &cpu->B);
	case 0x51:
		return LD_r_r(cpu, &cpu->D, &cpu->C);
	case 0x52:
		return LD_r_r(cpu, &cpu->D, &cpu->D);
	case 0x53:
		return LD_r_r(cpu, &cpu->D, &cpu->E);
	case 0x54:
		return LD_r_r(cpu, &cpu->D, &cpu->H);
	case 0x55:
		return LD_r_r(cpu, &cpu->D, &cpu->L);
	case 0x56:
		return LD_r_cHL(cpu, &cpu->D);
	case 0x57:
		return LD_r_r(cpu, &cpu->D, &cpu->A);
	case 0x58:
		return LD_r_r(cpu, &cpu->E, &cpu->B);
	case 0x59:
		return LD_r_r(cpu, &cpu->E, &cpu->C);
	case 0x5A:
		return LD_r_r(cpu, &cpu->E, &cpu->D);
	case 0x5B:
		return LD_r_r(cpu, &cpu->E, &cpu->E);
	case 0x5C:
		return LD_r_r(cpu, &cpu->E, &cpu->H);
	case 0x5D:
		return LD_r_r(cpu, &cpu->E, &cpu->L);
	case 0x5E:
		return LD_r_cHL(cpu, &cpu->E);
	case 0x5F:
		return LD_r_r(cpu, &cpu->E, &cpu->A);
	case 0x60:
		return LD_r_r(cpu, &cpu->H, &cpu->B);
	case 0x61:
		return LD_r_r(cpu, &cpu->H, &cpu->C);
	case 0x62:
		return LD_r_r(cpu, &cpu->H, &cpu->D);
	case 0x63:
		return LD_r_r(cpu, &cpu->H, &cpu->E);
	case 0x64:
		return LD_r_r(cpu, &cpu->H, &cpu->H);
	case 0x65:
		return LD_r_r(cpu, &cpu->H, &cpu->L);
	case 0x66:
		return LD_r_cHL(cpu, &cpu->H);
	case 0x67:
		return LD_r_r(cpu, &cpu->H, &cpu->A);
	case 0x68:
		return LD_r_r(cpu, &cpu->L, &cpu->B);
	case 0x69:
		return LD_r_r(cpu, &cpu->L, &cpu->C);
	case 0x6A:
		return LD_r_r(cpu, &cpu->L, &cpu->D);
	case 0x6B:
		return LD_r_r(cpu, &cpu->L, &cpu->E);
	case 0x6C:
		return LD_r_r(cpu, &cpu->L, &cpu->H);
	case 0x6D:
		return LD_r_r(cpu, &cpu->L, &cpu->L);
	case 0x6E:
		return LD_r_cHL(cpu, &cpu->L);
	case 0x6F:
		return LD_r_r(cpu, &cpu->L, &cpu->A);
	case 0x70:
		return LD_cHL_r(cpu, &cpu->B);
	case 0x71:
		return LD_cHL_r(cpu, &cpu->C);
	case 0x72:
		return LD_cHL_r(cpu, &cpu->D);
	case 0x73:
		return LD_cHL_r(cpu, &cpu->E);
	case 0x74:
		return LD_cHL_r(cpu, &cpu->H);
	case 0x75:
		return LD_cHL_r(cpu, &cpu->L);
	case 0x76:
		return HALT(cpu);
	case 0x77:
		return LD_cHL_r(cpu, &cpu->A);
	case 0x78:
		return LD_r_r(cpu, &cpu->A, &cpu->B);
	case 0x79:
		return LD_r_r(cpu, &cpu->A, &cpu->C);
	case 0x7A:
		return LD_r_r(cpu, &cpu->A, &cpu->D);
	case 0x7B:
		return LD_r_r(cpu, &cpu->A, &cpu->E);
	case 0x7C:
		return LD_r_r(cpu, &cpu->A, &cpu->H);
	case 0x7D:
		return LD_r_r(cpu, &cpu->A, &cpu->L);
	case 0x7E:
		return LD_r_cHL(cpu, &cpu->A);
	case 0x7F:
		return LD_r_r(cpu, &cpu->A, &cpu->A);
	case 0x80:
		return ADD_A_r(cpu, &cpu->B);
	case 0x81:
		return ADD_A_r(cpu, &cpu->C);
	case 0x82:
		return ADD_A_r(cpu, &cpu->D);
	case 0x83:
		return ADD_A_r(cpu, &cpu->E);
	case 0x84:
		return ADD_A_r(cpu, &cpu->H);
	case 0x85:
		return ADD_A_r(cpu, &cpu->L);
	case 0x86:
		return ADD_A_cHL(cpu);
	case 0x87:
		return ADD_A_r(cpu, &cpu->A);
	case 0x88:
		return ADC_A_r(cpu, &cpu->B);
	case 0x89:
		return ADC_A_r(cpu, &cpu->C);
	case 0x8A:
		return ADC_A_r(cpu, &cpu->D);
	case 0x8B:
		return ADC_A_r(cpu, &cpu->E);
	case 0x8C:
		return ADC_A_r(cpu, &cpu->H);
	case 0x8D:
		return ADC_A_r(cpu, &cpu->L);
	case 0x8E:
		return ADC_A_cHL(cpu);
	case 0x8F:
		return ADC_A_r(cpu, &cpu->A);
	case 0x90:
		return SUB_A_r(cpu, &cpu->B);
	case 0x91:
		return SUB_A_r(cpu, &cpu->C);
	case 0x92:
		return SUB_A_r(cpu, &cpu->D);
	case 0x93:
		return SUB_A_r(cpu, &cpu->E);
	case 0x94:
		return SUB_A_r(cpu, &cpu->H);
	case 0x95:
		return SUB_A_r(cpu, &cpu->L);
	case 0x96:
		return SUB_A_cHL(cpu);
	case 0x97:
		return SUB_A_r(cpu, &cpu->A);
	case 0x98:
		return SBC_A_r(cpu, &cpu->B);
	case 0x99:
		return SBC_A_r(cpu, &cpu->C);
	case 0x9A:
		return SBC_A_r(cpu, &cpu->D);
	case 0x9B:
		return SBC_A_r(cpu, &cpu->E);
	case 0x9C:
		return SBC_A_r(cpu, &cpu->H);
	case 0x9D:
		return SBC_A_r(cpu, &cpu->L);
	case 0x9E:
		return SBC_A_cHL(cpu);
	case 0x9F:
		return SBC_A_r(cpu, &cpu->A);
	case 0xA0:
		return AND_r(cpu, &cpu->B);
	case 0xA1:
		return AND_r(cpu, &cpu->C);
	case 0xA2:
		return AND_r(cpu, &cpu->D);
	case 0xA3:
		return AND_r(cpu, &cpu->E);
	case 0xA4:
		return AND_r(cpu, &cpu->H);
	case 0xA5:
		return AND_r(cpu, &cpu->L);
	case 0xA6:
		return AND_cHL(cpu);
	case 0xA7:
		return AND_r(cpu, &cpu->A);
	case 0xA8:
		return XOR_r(cpu, &cpu->B);
	case 0xA9:
		return XOR_r(cpu, &cpu->C);
	case 0xAA:
		return XOR_r(cpu, &cpu->D);
	case 0xAB:
		return XOR_r(cpu, &cpu->E);
	case 0xAC:
		return XOR_r(cpu, &cpu->H);
	case 0xAD:
		return XOR_r(cpu, &cpu->L);
	case 0xAE:
		return XOR_cHL(cpu);
	case 0xAF:
		return XOR_r(cpu, &cpu->A);
	case 0xB0:
		return OR_r(cpu, &cpu->B);
	case 0xB1:
		return OR_r(cpu, &cpu->C);
	case 0xB2:
		return OR_r(cpu, &cpu->D);
	case 0xB3:
		return OR_r(cpu, &cpu->E);
	case 0xB4:
		return OR_r(cpu, &cpu->H);
	case 0xB5:
		return OR_r(cpu, &cpu->L);
	case 0xB6:
		return OR_cHL(cpu);
	case 0xB7:
		return OR_r(cpu, &cpu->A);
	case 0xB8:
		return CP_r(cpu, &cpu->B);
	case 0xB9:
		return CP_r(cpu, &cpu->C);
	case 0xBA:
		return CP_r(cpu, &cpu->D);
	case 0xBB:
		return CP_r(cpu, &cpu->E);
	case 0xBC:
		return CP_r(cpu, &cpu->H);
	case 0xBD:
		return CP_r(cpu, &cpu->L);
	case 0xBE:
		return CP_cHL(cpu);
	case 0xBF:
		return CP_r(cpu, &cpu->A);
	case 0xC0:
		return RET_NZ(cpu);
	case 0xC1:
		return POP_rr(cpu, &cpu->BC);
	case 0xC2:
		return JP_NZ_nn(cpu);
	case 0xC3:
		return JP_nn(cpu);
	case 0xC4:
		return CALL_NZ_nn(cpu);
	case 0xC5:
		return PUSH_rr(cpu, &cpu->BC);
	case 0xC6:
		return ADD_A_n(cpu);
	case 0xC7:
		return RST_n(cpu, 0x00);
	case 0xC8:
		return RET_Z(cpu);
	case 0xC9:
		return RET(cpu);
	case 0xCA:
		return JP_Z_nn(cpu);
	case 0xCB:
		return lr35902_opcode_CB(cpu);
	case 0xCC:
		return CALL_Z_nn(cpu);
	case 0xCD:
		return CALL_nn(cpu);
	case 0xCE:
		return ADC_A_n(cpu);
	case 0xCF:
		return RST_n(cpu, 0x08);
	case 0xD0:
		return RET_NC(cpu);
	case 0xD1:
		return POP_rr(cpu, &cpu->DE);
	case 0xD2:
		return JP_NC_nn(cpu);
	case 0xD4:
		return CALL_NC_nn(cpu);
	case 0xD5:
		return PUSH_rr(cpu, &cpu->DE);
	case 0xD6:
		return SUB_A_n(cpu);
	case 0xD7:
		return RST_n(cpu, 0x10);
	case 0xD8:
		return RET_C(cpu);
	case 0xD9:
		return RETI(cpu);
	case 0xDA:
		return JP_C_nn(cpu);
	case 0xDC:
		return CALL_C_nn(cpu);
	case 0xDE:
		return SBC_A_n(cpu);
	case 0xDF:
		return RST_n(cpu, 0x18);
	case 0xE0:
		return LD_cFF00pn_A(cpu);
	case 0xE1:
		return POP_rr(cpu, &cpu->HL);
	case 0xE2:
		return LD_cFF00pC_A(cpu);
	case 0xE5:
		return PUSH_rr(cpu, &cpu->HL);
	case 0xE6:
		return AND_n(cpu);
	case 0xE7:
		return RST_n(cpu, 0x20);
	case 0xE8:
		return ADD_SP_d(cpu);
	case 0xE9:
		return JP_HL(cpu);
	case 0xEA:
		return LD_cnn_A(cpu);
	case 0xEE:
		return XOR_n(cpu);
	case 0xEF:
		return RST_n(cpu, 0x28);
	case 0xF0:
		return LD_A_cFF00pn(cpu);
	case 0xF1:
		return POP_AF(cpu);
	case 0xF2:
		return LD_A_cFF00pC(cpu);
	case 0xF3:
		return DI(cpu);
	case 0xF5:
		return PUSH_rr(cpu, &cpu->AF);
	case 0xF6:
		return OR_n(cpu);
	case 0xF7:
		return RST_n(cpu, 0x30);
	case 0xF8:
		return LD_HL_SPpd(cpu);
	case 0xF9:
		return LD_SP_HL(cpu);
	case 0xFA:
		return LD_A_cnn(cpu);
	case 0xFB:
		return EI(cpu);
	case 0xFE:
		return CP_n(cpu);
	case 0xFF:
		return RST_n(cpu, 0x38);
	default:
		fprintf(stderr, "lr35902: unknown opcode (%02x)!\n", opcode);
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
		return RLC_r(cpu, &cpu->B);
	case 0x01:
		return RLC_r(cpu, &cpu->C);
	case 0x02:
		return RLC_r(cpu, &cpu->D);
	case 0x03:
		return RLC_r(cpu, &cpu->E);
	case 0x04:
		return RLC_r(cpu, &cpu->H);
	case 0x05:
		return RLC_r(cpu, &cpu->L);
	case 0x06:
		return RLC_cHL(cpu);
	case 0x07:
		return RLC_r(cpu, &cpu->A);
	case 0x08:
		return RRC_r(cpu, &cpu->B);
	case 0x09:
		return RRC_r(cpu, &cpu->C);
	case 0x0A:
		return RRC_r(cpu, &cpu->D);
	case 0x0B:
		return RRC_r(cpu, &cpu->E);
	case 0x0C:
		return RRC_r(cpu, &cpu->H);
	case 0x0D:
		return RRC_r(cpu, &cpu->L);
	case 0x0E:
		return RRC_cHL(cpu);
	case 0x0F:
		return RRC_r(cpu, &cpu->A);
	case 0x10:
		return RL_r(cpu, &cpu->B);
	case 0x11:
		return RL_r(cpu, &cpu->C);
	case 0x12:
		return RL_r(cpu, &cpu->D);
	case 0x13:
		return RL_r(cpu, &cpu->E);
	case 0x14:
		return RL_r(cpu, &cpu->H);
	case 0x15:
		return RL_r(cpu, &cpu->L);
	case 0x16:
		return RL_cHL(cpu);
	case 0x17:
		return RL_r(cpu, &cpu->A);
	case 0x18:
		return RR_r(cpu, &cpu->B);
	case 0x19:
		return RR_r(cpu, &cpu->C);
	case 0x1A:
		return RR_r(cpu, &cpu->D);
	case 0x1B:
		return RR_r(cpu, &cpu->E);
	case 0x1C:
		return RR_r(cpu, &cpu->H);
	case 0x1D:
		return RR_r(cpu, &cpu->L);
	case 0x1E:
		return RR_cHL(cpu);
	case 0x1F:
		return RR_r(cpu, &cpu->A);
	case 0x20:
		return SLA_r(cpu, &cpu->B);
	case 0x21:
		return SLA_r(cpu, &cpu->C);
	case 0x22:
		return SLA_r(cpu, &cpu->D);
	case 0x23:
		return SLA_r(cpu, &cpu->E);
	case 0x24:
		return SLA_r(cpu, &cpu->H);
	case 0x25:
		return SLA_r(cpu, &cpu->L);
	case 0x26:
		return SLA_cHL(cpu);
	case 0x27:
		return SLA_r(cpu, &cpu->A);
	case 0x28:
		return SRA_r(cpu, &cpu->B);
	case 0x29:
		return SRA_r(cpu, &cpu->C);
	case 0x2A:
		return SRA_r(cpu, &cpu->D);
	case 0x2B:
		return SRA_r(cpu, &cpu->E);
	case 0x2C:
		return SRA_r(cpu, &cpu->H);
	case 0x2D:
		return SRA_r(cpu, &cpu->L);
	case 0x2E:
		return SRA_cHL(cpu);
	case 0x2F:
		return SRA_r(cpu, &cpu->A);
	case 0x30:
		return SWAP_r(cpu, &cpu->B);
	case 0x31:
		return SWAP_r(cpu, &cpu->C);
	case 0x32:
		return SWAP_r(cpu, &cpu->D);
	case 0x33:
		return SWAP_r(cpu, &cpu->E);
	case 0x34:
		return SWAP_r(cpu, &cpu->H);
	case 0x35:
		return SWAP_r(cpu, &cpu->L);
	case 0x36:
		return SWAP_cHL(cpu);
	case 0x37:
		return SWAP_r(cpu, &cpu->A);
	case 0x38:
		return SRL_r(cpu, &cpu->B);
	case 0x39:
		return SRL_r(cpu, &cpu->C);
	case 0x3A:
		return SRL_r(cpu, &cpu->D);
	case 0x3B:
		return SRL_r(cpu, &cpu->E);
	case 0x3C:
		return SRL_r(cpu, &cpu->H);
	case 0x3D:
		return SRL_r(cpu, &cpu->L);
	case 0x3E:
		return SRL_cHL(cpu);
	case 0x3F:
		return SRL_r(cpu, &cpu->A);
	case 0x40:
		return BIT_n_r(cpu, 0, &cpu->B);
	case 0x41:
		return BIT_n_r(cpu, 0, &cpu->C);
	case 0x42:
		return BIT_n_r(cpu, 0, &cpu->D);
	case 0x43:
		return BIT_n_r(cpu, 0, &cpu->E);
	case 0x44:
		return BIT_n_r(cpu, 0, &cpu->H);
	case 0x45:
		return BIT_n_r(cpu, 0, &cpu->L);
	case 0x46:
		return BIT_n_cHL(cpu, 0);
	case 0x47:
		return BIT_n_r(cpu, 0, &cpu->A);
	case 0x48:
		return BIT_n_r(cpu, 1, &cpu->B);
	case 0x49:
		return BIT_n_r(cpu, 1, &cpu->C);
	case 0x4A:
		return BIT_n_r(cpu, 1, &cpu->D);
	case 0x4B:
		return BIT_n_r(cpu, 1, &cpu->E);
	case 0x4C:
		return BIT_n_r(cpu, 1, &cpu->H);
	case 0x4D:
		return BIT_n_r(cpu, 1, &cpu->L);
	case 0x4E:
		return BIT_n_cHL(cpu, 1);
	case 0x4F:
		return BIT_n_r(cpu, 1, &cpu->A);
	case 0x50:
		return BIT_n_r(cpu, 2, &cpu->B);
	case 0x51:
		return BIT_n_r(cpu, 2, &cpu->C);
	case 0x52:
		return BIT_n_r(cpu, 2, &cpu->D);
	case 0x53:
		return BIT_n_r(cpu, 2, &cpu->E);
	case 0x54:
		return BIT_n_r(cpu, 2, &cpu->H);
	case 0x55:
		return BIT_n_r(cpu, 2, &cpu->L);
	case 0x56:
		return BIT_n_cHL(cpu, 2);
	case 0x57:
		return BIT_n_r(cpu, 2, &cpu->A);
	case 0x58:
		return BIT_n_r(cpu, 3, &cpu->B);
	case 0x59:
		return BIT_n_r(cpu, 3, &cpu->C);
	case 0x5A:
		return BIT_n_r(cpu, 3, &cpu->D);
	case 0x5B:
		return BIT_n_r(cpu, 3, &cpu->E);
	case 0x5C:
		return BIT_n_r(cpu, 3, &cpu->H);
	case 0x5D:
		return BIT_n_r(cpu, 3, &cpu->L);
	case 0x5E:
		return BIT_n_cHL(cpu, 3);
	case 0x5F:
		return BIT_n_r(cpu, 3, &cpu->A);
	case 0x60:
		return BIT_n_r(cpu, 4, &cpu->B);
	case 0x61:
		return BIT_n_r(cpu, 4, &cpu->C);
	case 0x62:
		return BIT_n_r(cpu, 4, &cpu->D);
	case 0x63:
		return BIT_n_r(cpu, 4, &cpu->E);
	case 0x64:
		return BIT_n_r(cpu, 4, &cpu->H);
	case 0x65:
		return BIT_n_r(cpu, 4, &cpu->L);
	case 0x66:
		return BIT_n_cHL(cpu, 4);
	case 0x67:
		return BIT_n_r(cpu, 4, &cpu->A);
	case 0x68:
		return BIT_n_r(cpu, 5, &cpu->B);
	case 0x69:
		return BIT_n_r(cpu, 5, &cpu->C);
	case 0x6A:
		return BIT_n_r(cpu, 5, &cpu->D);
	case 0x6B:
		return BIT_n_r(cpu, 5, &cpu->E);
	case 0x6C:
		return BIT_n_r(cpu, 5, &cpu->H);
	case 0x6D:
		return BIT_n_r(cpu, 5, &cpu->L);
	case 0x6E:
		return BIT_n_cHL(cpu, 5);
	case 0x6F:
		return BIT_n_r(cpu, 5, &cpu->A);
	case 0x70:
		return BIT_n_r(cpu, 6, &cpu->B);
	case 0x71:
		return BIT_n_r(cpu, 6, &cpu->C);
	case 0x72:
		return BIT_n_r(cpu, 6, &cpu->D);
	case 0x73:
		return BIT_n_r(cpu, 6, &cpu->E);
	case 0x74:
		return BIT_n_r(cpu, 6, &cpu->H);
	case 0x75:
		return BIT_n_r(cpu, 6, &cpu->L);
	case 0x76:
		return BIT_n_cHL(cpu, 6);
	case 0x77:
		return BIT_n_r(cpu, 6, &cpu->A);
	case 0x78:
		return BIT_n_r(cpu, 7, &cpu->B);
	case 0x79:
		return BIT_n_r(cpu, 7, &cpu->C);
	case 0x7A:
		return BIT_n_r(cpu, 7, &cpu->D);
	case 0x7B:
		return BIT_n_r(cpu, 7, &cpu->E);
	case 0x7C:
		return BIT_n_r(cpu, 7, &cpu->H);
	case 0x7D:
		return BIT_n_r(cpu, 7, &cpu->L);
	case 0x7E:
		return BIT_n_cHL(cpu, 7);
	case 0x7F:
		return BIT_n_r(cpu, 7, &cpu->A);
	case 0x80:
		return RES_n_r(cpu, 0, &cpu->B);
	case 0x81:
		return RES_n_r(cpu, 0, &cpu->C);
	case 0x82:
		return RES_n_r(cpu, 0, &cpu->D);
	case 0x83:
		return RES_n_r(cpu, 0, &cpu->E);
	case 0x84:
		return RES_n_r(cpu, 0, &cpu->H);
	case 0x85:
		return RES_n_r(cpu, 0, &cpu->L);
	case 0x86:
		return RES_n_cHL(cpu, 0);
	case 0x87:
		return RES_n_r(cpu, 0, &cpu->A);
	case 0x88:
		return RES_n_r(cpu, 1, &cpu->B);
	case 0x89:
		return RES_n_r(cpu, 1, &cpu->C);
	case 0x8A:
		return RES_n_r(cpu, 1, &cpu->D);
	case 0x8B:
		return RES_n_r(cpu, 1, &cpu->E);
	case 0x8C:
		return RES_n_r(cpu, 1, &cpu->H);
	case 0x8D:
		return RES_n_r(cpu, 1, &cpu->L);
	case 0x8E:
		return RES_n_cHL(cpu, 1);
	case 0x8F:
		return RES_n_r(cpu, 1, &cpu->A);
	case 0x90:
		return RES_n_r(cpu, 2, &cpu->B);
	case 0x91:
		return RES_n_r(cpu, 2, &cpu->C);
	case 0x92:
		return RES_n_r(cpu, 2, &cpu->D);
	case 0x93:
		return RES_n_r(cpu, 2, &cpu->E);
	case 0x94:
		return RES_n_r(cpu, 2, &cpu->H);
	case 0x95:
		return RES_n_r(cpu, 2, &cpu->L);
	case 0x96:
		return RES_n_cHL(cpu, 2);
	case 0x97:
		return RES_n_r(cpu, 2, &cpu->A);
	case 0x98:
		return RES_n_r(cpu, 3, &cpu->B);
	case 0x99:
		return RES_n_r(cpu, 3, &cpu->C);
	case 0x9A:
		return RES_n_r(cpu, 3, &cpu->D);
	case 0x9B:
		return RES_n_r(cpu, 3, &cpu->E);
	case 0x9C:
		return RES_n_r(cpu, 3, &cpu->H);
	case 0x9D:
		return RES_n_r(cpu, 3, &cpu->L);
	case 0x9E:
		return RES_n_cHL(cpu, 3);
	case 0x9F:
		return RES_n_r(cpu, 3, &cpu->A);
	case 0xA0:
		return RES_n_r(cpu, 4, &cpu->B);
	case 0xA1:
		return RES_n_r(cpu, 4, &cpu->C);
	case 0xA2:
		return RES_n_r(cpu, 4, &cpu->D);
	case 0xA3:
		return RES_n_r(cpu, 4, &cpu->E);
	case 0xA4:
		return RES_n_r(cpu, 4, &cpu->H);
	case 0xA5:
		return RES_n_r(cpu, 4, &cpu->L);
	case 0xA6:
		return RES_n_cHL(cpu, 4);
	case 0xA7:
		return RES_n_r(cpu, 4, &cpu->A);
	case 0xA8:
		return RES_n_r(cpu, 5, &cpu->B);
	case 0xA9:
		return RES_n_r(cpu, 5, &cpu->C);
	case 0xAA:
		return RES_n_r(cpu, 5, &cpu->D);
	case 0xAB:
		return RES_n_r(cpu, 5, &cpu->E);
	case 0xAC:
		return RES_n_r(cpu, 5, &cpu->H);
	case 0xAD:
		return RES_n_r(cpu, 5, &cpu->L);
	case 0xAE:
		return RES_n_cHL(cpu, 5);
	case 0xAF:
		return RES_n_r(cpu, 5, &cpu->A);
	case 0xB0:
		return RES_n_r(cpu, 6, &cpu->B);
	case 0xB1:
		return RES_n_r(cpu, 6, &cpu->C);
	case 0xB2:
		return RES_n_r(cpu, 6, &cpu->D);
	case 0xB3:
		return RES_n_r(cpu, 6, &cpu->E);
	case 0xB4:
		return RES_n_r(cpu, 6, &cpu->H);
	case 0xB5:
		return RES_n_r(cpu, 6, &cpu->L);
	case 0xB6:
		return RES_n_cHL(cpu, 6);
	case 0xB7:
		return RES_n_r(cpu, 6, &cpu->A);
	case 0xB8:
		return RES_n_r(cpu, 7, &cpu->B);
	case 0xB9:
		return RES_n_r(cpu, 7, &cpu->C);
	case 0xBA:
		return RES_n_r(cpu, 7, &cpu->D);
	case 0xBB:
		return RES_n_r(cpu, 7, &cpu->E);
	case 0xBC:
		return RES_n_r(cpu, 7, &cpu->H);
	case 0xBD:
		return RES_n_r(cpu, 7, &cpu->L);
	case 0xBE:
		return RES_n_cHL(cpu, 7);
	case 0xBF:
		return RES_n_r(cpu, 7, &cpu->A);
	case 0xC0:
		return SET_n_r(cpu, 0, &cpu->B);
	case 0xC1:
		return SET_n_r(cpu, 0, &cpu->C);
	case 0xC2:
		return SET_n_r(cpu, 0, &cpu->D);
	case 0xC3:
		return SET_n_r(cpu, 0, &cpu->E);
	case 0xC4:
		return SET_n_r(cpu, 0, &cpu->H);
	case 0xC5:
		return SET_n_r(cpu, 0, &cpu->L);
	case 0xC6:
		return SET_n_cHL(cpu, 0);
	case 0xC7:
		return SET_n_r(cpu, 0, &cpu->A);
	case 0xC8:
		return SET_n_r(cpu, 1, &cpu->B);
	case 0xC9:
		return SET_n_r(cpu, 1, &cpu->C);
	case 0xCA:
		return SET_n_r(cpu, 1, &cpu->D);
	case 0xCB:
		return SET_n_r(cpu, 1, &cpu->E);
	case 0xCC:
		return SET_n_r(cpu, 1, &cpu->H);
	case 0xCD:
		return SET_n_r(cpu, 1, &cpu->L);
	case 0xCE:
		return SET_n_cHL(cpu, 1);
	case 0xCF:
		return SET_n_r(cpu, 1, &cpu->A);
	case 0xD0:
		return SET_n_r(cpu, 2, &cpu->B);
	case 0xD1:
		return SET_n_r(cpu, 2, &cpu->C);
	case 0xD2:
		return SET_n_r(cpu, 2, &cpu->D);
	case 0xD3:
		return SET_n_r(cpu, 2, &cpu->E);
	case 0xD4:
		return SET_n_r(cpu, 2, &cpu->H);
	case 0xD5:
		return SET_n_r(cpu, 2, &cpu->L);
	case 0xD6:
		return SET_n_cHL(cpu, 2);
	case 0xD7:
		return SET_n_r(cpu, 2, &cpu->A);
	case 0xD8:
		return SET_n_r(cpu, 3, &cpu->B);
	case 0xD9:
		return SET_n_r(cpu, 3, &cpu->C);
	case 0xDA:
		return SET_n_r(cpu, 3, &cpu->D);
	case 0xDB:
		return SET_n_r(cpu, 3, &cpu->E);
	case 0xDC:
		return SET_n_r(cpu, 3, &cpu->H);
	case 0xDD:
		return SET_n_r(cpu, 3, &cpu->L);
	case 0xDE:
		return SET_n_cHL(cpu, 3);
	case 0xDF:
		return SET_n_r(cpu, 3, &cpu->A);
	case 0xE0:
		return SET_n_r(cpu, 4, &cpu->B);
	case 0xE1:
		return SET_n_r(cpu, 4, &cpu->C);
	case 0xE2:
		return SET_n_r(cpu, 4, &cpu->D);
	case 0xE3:
		return SET_n_r(cpu, 4, &cpu->E);
	case 0xE4:
		return SET_n_r(cpu, 4, &cpu->H);
	case 0xE5:
		return SET_n_r(cpu, 4, &cpu->L);
	case 0xE6:
		return SET_n_cHL(cpu, 4);
	case 0xE7:
		return SET_n_r(cpu, 4, &cpu->A);
	case 0xE8:
		return SET_n_r(cpu, 5, &cpu->B);
	case 0xE9:
		return SET_n_r(cpu, 5, &cpu->C);
	case 0xEA:
		return SET_n_r(cpu, 5, &cpu->D);
	case 0xEB:
		return SET_n_r(cpu, 5, &cpu->E);
	case 0xEC:
		return SET_n_r(cpu, 5, &cpu->H);
	case 0xED:
		return SET_n_r(cpu, 5, &cpu->L);
	case 0xEE:
		return SET_n_cHL(cpu, 5);
	case 0xEF:
		return SET_n_r(cpu, 5, &cpu->A);
	case 0xF0:
		return SET_n_r(cpu, 6, &cpu->B);
	case 0xF1:
		return SET_n_r(cpu, 6, &cpu->C);
	case 0xF2:
		return SET_n_r(cpu, 6, &cpu->D);
	case 0xF3:
		return SET_n_r(cpu, 6, &cpu->E);
	case 0xF4:
		return SET_n_r(cpu, 6, &cpu->H);
	case 0xF5:
		return SET_n_r(cpu, 6, &cpu->L);
	case 0xF6:
		return SET_n_cHL(cpu, 6);
	case 0xF7:
		return SET_n_r(cpu, 6, &cpu->A);
	case 0xF8:
		return SET_n_r(cpu, 7, &cpu->B);
	case 0xF9:
		return SET_n_r(cpu, 7, &cpu->C);
	case 0xFA:
		return SET_n_r(cpu, 7, &cpu->D);
	case 0xFB:
		return SET_n_r(cpu, 7, &cpu->E);
	case 0xFC:
		return SET_n_r(cpu, 7, &cpu->H);
	case 0xFD:
		return SET_n_r(cpu, 7, &cpu->L);
	case 0xFE:
		return SET_n_cHL(cpu, 7);
	case 0xFF:
		return SET_n_r(cpu, 7, &cpu->A);
	default:
		fprintf(stderr, "lr35902: unknown opcode (%02x)!\n", opcode);
		clock_consume(1);
		break;
	}
}

bool lr35902_init(struct cpu_instance *instance)
{
	struct lr35902 *cpu;
	struct resource *res;

	/* Allocate lr35902 structure and set private data */
	cpu = malloc(sizeof(struct lr35902));
	instance->priv_data = cpu;

	/* Initialize processor data */
	cpu->bus_id = instance->bus_id;
	cpu->PC = 0;

	/* Add CPU clock */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	cpu->clock.rate = res->data.clk;
	cpu->clock.data = cpu;
	cpu->clock.tick = lr35902_tick;
	clock_add(&cpu->clock);

	return true;
}

void lr35902_interrupt(struct cpu_instance *UNUSED(instance), int UNUSED(irq))
{
}

void lr35902_deinit(struct cpu_instance *instance)
{
	struct lr35902 *cpu = instance->priv_data;
	free(cpu);
}

CPU_START(lr35902)
	.init = lr35902_init,
	.interrupt = lr35902_interrupt,
	.deinit = lr35902_deinit
CPU_END

