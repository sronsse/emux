#include <stdio.h>
#include <stdlib.h>
#include <bitops.h>
#include <clock.h>
#include <cpu.h>
#include <log.h>
#include <memory.h>
#include <port.h>
#include <util.h>

#define DEFINE_AF_PAIR \
	union { \
		struct { \
			union { \
				uint8_t F; \
				struct z80_flags flags; \
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

#define DEFINE_IXY_REGISTER_PAIR(IXY) \
	union { \
		struct { \
			uint8_t l##IXY; \
			uint8_t h##IXY; \
		}; \
		uint16_t IXY; \
	};

#define IRQ_N		0
#define NMI_N		1
#define IRQ_VECTOR	0x0038
#define NMI_VECTOR	0x0066

struct z80_flags {
	uint8_t C:1;
	uint8_t N:1;
	uint8_t PV:1;
	uint8_t X:1;
	uint8_t H:1;
	uint8_t Y:1;
	uint8_t Z:1;
	uint8_t S:1;
};

struct z80 {
	DEFINE_AF_PAIR
	DEFINE_REGISTER_PAIR(A2, F2)
	DEFINE_REGISTER_PAIR(B, C)
	DEFINE_REGISTER_PAIR(B2, C2)
	DEFINE_REGISTER_PAIR(D, E)
	DEFINE_REGISTER_PAIR(D2, E2)
	DEFINE_REGISTER_PAIR(H, L)
	DEFINE_REGISTER_PAIR(H2, L2)
	DEFINE_IXY_REGISTER_PAIR(IX);
	DEFINE_IXY_REGISTER_PAIR(IY);
	uint16_t PC;
	uint16_t SP;
	uint8_t I;
	uint8_t R;
	bool IFF1;
	bool IFF2;
	bool irq_delay;
	uint8_t interrupt_mode;
	bool irq_pending;
	bool nmi_pending;
	bool halted;
	int bus_id;
	struct clock clock;
};

static bool z80_init(struct cpu_instance *instance);
static void z80_reset(struct cpu_instance *instance);
static void z80_interrupt(struct cpu_instance *instance, int irq);
static void z80_deinit(struct cpu_instance *instance);
static bool z80_handle_irq(struct z80 *cpu);
static bool z80_handle_nmi(struct z80 *cpu);
static void z80_tick(struct z80 *cpu);
static void z80_opcode_CB(struct z80 *cpu);
static void z80_opcode_DDFD(struct z80 *cpu, uint8_t prefix);
static void z80_opcode_DDFD_CB(struct z80 *cpu, uint16_t *reg);
static void z80_opcode_ED(struct z80 *cpu);
static inline void LD_r_r(uint8_t *r1, uint8_t *r2);
static inline void LD_r_n(struct z80 *cpu, uint8_t *r);
static inline void LD_r_cHL(struct z80 *cpu, uint8_t *r);
static inline void LD_r_cIXYpd(struct z80 *cpu, uint8_t *r, uint16_t *reg);
static inline void LD_cHL_r(struct z80 *cpu, uint8_t *r);
static inline void LD_cIXYpd_r(struct z80 *cpu, uint8_t *r, uint16_t *reg);
static inline void LD_cHL_n(struct z80 *cpu);
static inline void LD_cIXYpd_n(struct z80 *cpu, uint16_t *reg);
static inline void LD_A_cBC(struct z80 *cpu);
static inline void LD_A_cDE(struct z80 *cpu);
static inline void LD_A_cnn(struct z80 *cpu);
static inline void LD_cBC_A(struct z80 *cpu);
static inline void LD_cDE_A(struct z80 *cpu);
static inline void LD_cnn_A(struct z80 *cpu);
static inline void LD_A_I(struct z80 *cpu);
static inline void LD_A_R(struct z80 *cpu);
static inline void LD_I_A(struct z80 *cpu);
static inline void LD_R_A(struct z80 *cpu);
static inline void LD_dd_nn(struct z80 *cpu, uint16_t *dd);
static inline void LD_IXY_nn(struct z80 *cpu, uint16_t *reg);
static inline void LD_HL_cnn(struct z80 *cpu);
static inline void LD_dd_cnn(struct z80 *cpu, uint16_t *dd);
static inline void LD_IXY_cnn(struct z80 *cpu, uint16_t *reg);
static inline void LD_cnn_HL(struct z80 *cpu);
static inline void LD_cnn_dd(struct z80 *cpu, uint16_t *dd);
static inline void LD_cnn_IXY(struct z80 *cpu, uint16_t *reg);
static inline void LD_SP_HL(struct z80 *cpu);
static inline void LD_SP_IXY(struct z80 *cpu, uint16_t *reg);
static inline void PUSH_qq(struct z80 *cpu, uint16_t *qq);
static inline void PUSH_IXY(struct z80 *cpu, uint16_t *reg);
static inline void POP_qq(struct z80 *cpu, uint16_t *qq);
static inline void POP_IXY(struct z80 *cpu, uint16_t *reg);
static inline void EX_DE_HL(struct z80 *cpu);
static inline void EX_AF_A2F2(struct z80 *cpu);
static inline void EXX(struct z80 *cpu);
static inline void EX_cSP_HL(struct z80 *cpu);
static inline void EX_cSP_IXY(struct z80 *cpu, uint16_t *reg);
static inline void LDI(struct z80 *cpu);
static inline void LDIR(struct z80 *cpu);
static inline void LDD(struct z80 *cpu);
static inline void LDDR(struct z80 *cpu);
static inline void CPI(struct z80 *cpu);
static inline void CPIR(struct z80 *cpu);
static inline void CPD(struct z80 *cpu);
static inline void CPDR(struct z80 *cpu);
static inline void ADD_A_r(struct z80 *cpu, uint8_t *r);
static inline void ADD_A_n(struct z80 *cpu);
static inline void ADD_A_cHL(struct z80 *cpu);
static inline void ADD_A_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void ADC_A_r(struct z80 *cpu, uint8_t *r);
static inline void ADC_A_n(struct z80 *cpu);
static inline void ADC_A_cHL(struct z80 *cpu);
static inline void ADC_A_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void SUB_A_r(struct z80 *cpu, uint8_t *r);
static inline void SUB_A_n(struct z80 *cpu);
static inline void SUB_A_cHL(struct z80 *cpu);
static inline void SUB_A_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void SBC_A_r(struct z80 *cpu, uint8_t *r);
static inline void SBC_A_n(struct z80 *cpu);
static inline void SBC_A_cHL(struct z80 *cpu);
static inline void SBC_A_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void AND_r(struct z80 *cpu, uint8_t *r);
static inline void AND_n(struct z80 *cpu);
static inline void AND_cHL(struct z80 *cpu);
static inline void AND_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void OR_r(struct z80 *cpu, uint8_t *r);
static inline void OR_n(struct z80 *cpu);
static inline void OR_cHL(struct z80 *cpu);
static inline void OR_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void XOR_r(struct z80 *cpu, uint8_t *r);
static inline void XOR_n(struct z80 *cpu);
static inline void XOR_cHL(struct z80 *cpu);
static inline void XOR_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void CP_r(struct z80 *cpu, uint8_t *r);
static inline void CP_n(struct z80 *cpu);
static inline void CP_cHL(struct z80 *cpu);
static inline void CP_IXYpd(struct z80 *cpu, uint16_t *reg);
static inline void INC_r(struct z80 *cpu, uint8_t *r);
static inline void INC_cHL(struct z80 *cpu);
static inline void INC_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void DEC_r(struct z80 *cpu, uint8_t *r);
static inline void DEC_cHL(struct z80 *cpu);
static inline void DEC_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void DAA(struct z80 *cpu);
static inline void CPL(struct z80 *cpu);
static inline void NEG(struct z80 *cpu);
static inline void CCF(struct z80 *cpu);
static inline void SCF(struct z80 *cpu);
static inline void NOP(struct z80 *UNUSED(cpu));
static inline void HALT(struct z80 *cpu);
static inline void DI(struct z80 *cpu);
static inline void EI(struct z80 *cpu);
static inline void IM_0(struct z80 *cpu);
static inline void IM_1(struct z80 *cpu);
static inline void IM_2(struct z80 *cpu);
static inline void ADD_HL_ss(struct z80 *cpu, uint16_t *ss);
static inline void ADC_HL_rr(struct z80 *cpu, uint16_t *rr);
static inline void SBC_HL_rr(struct z80 *cpu, uint16_t *rr);
static inline void ADD_IXY_pp(struct z80 *cpu, uint16_t *rr, uint16_t *reg);
static inline void INC_ss(uint16_t *ss);
static inline void DEC_ss(uint16_t *rr);
static inline void DEC_IXY(uint16_t *reg);
static inline void RLCA(struct z80 *cpu);
static inline void RLA(struct z80 *cpu);
static inline void RRCA(struct z80 *cpu);
static inline void RRA(struct z80 *cpu);
static inline void RLC_r(struct z80 *cpu, uint8_t *r);
static inline void RLC_cHL(struct z80 *cpu);
static inline void RLC_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void RL_r(struct z80 *cpu, uint8_t *r);
static inline void RL_cHL(struct z80 *cpu);
static inline void RL_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void RRC_r(struct z80 *cpu, uint8_t *r);
static inline void RRC_cHL(struct z80 *cpu);
static inline void RRC_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void RR_r(struct z80 *cpu, uint8_t *r);
static inline void RR_cHL(struct z80 *cpu);
static inline void RR_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void SLA_r(struct z80 *cpu, uint8_t *r);
static inline void SLA_cHL(struct z80 *cpu);
static inline void SLA_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void SRA_r(struct z80 *cpu, uint8_t *r);
static inline void SRA_cHL(struct z80 *cpu);
static inline void SRA_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void SL1_r(struct z80 *cpu, uint8_t *r);
static inline void SL1_cHL(struct z80 *cpu);
static inline void SL1_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void SRL_r(struct z80 *cpu, uint8_t *r);
static inline void SRL_cHL(struct z80 *cpu);
static inline void SRL_cIXYpd(struct z80 *cpu, uint16_t *reg);
static inline void RLD(struct z80 *cpu);
static inline void RRD(struct z80 *cpu);
static inline void BIT_b_r(struct z80 *cpu, uint8_t b, uint8_t *r);
static inline void BIT_b_cHL(struct z80 *cpu, uint8_t b);
static inline void BIT_b_cIXYpd(struct z80 *cpu, uint8_t b, uint16_t *reg);
static inline void SET_b_r(uint8_t b, uint8_t *r);
static inline void SET_b_cHL(struct z80 *cpu, uint8_t b);
static inline void SET_b_cIXYpd(struct z80 *cpu, uint8_t b, uint16_t *reg);
static inline void RES_b_r(uint8_t b, uint8_t *r);
static inline void RES_b_cHL(struct z80 *cpu, uint8_t b);
static inline void RES_b_cIXYpd(struct z80 *cpu, uint8_t b, uint16_t *reg);
static inline void JP_nn(struct z80 *cpu);
static inline void JP_cc_nn(struct z80 *cpu, bool condition);
static inline void JR_e(struct z80 *cpu);
static inline void JR_cc_e(struct z80 *cpu, bool condition);
static inline void JP_HL(struct z80 *cpu);
static inline void JP_IXY(struct z80 *cpu, uint16_t *reg);
static inline void DJNZ_e(struct z80 *cpu);
static inline void CALL_nn(struct z80 *cpu);
static inline void CALL_cc_nn(struct z80 *cpu, bool condition);
static inline void RET(struct z80 *cpu);
static inline void RET_cc(struct z80 *cpu, bool condition);
static inline void RETI(struct z80 *cpu);
static inline void RETN(struct z80 *cpu);
static inline void RST_p(struct z80 *cpu, uint8_t p);
static inline void IN_A_cn(struct z80 *cpu);
static inline void IN_r_cC(struct z80 *cpu, uint8_t *r);
static inline void INI(struct z80 *cpu);
static inline void INIR(struct z80 *cpu);
static inline void IND(struct z80 *cpu);
static inline void INDR(struct z80 *cpu);
static inline void OUT_cn_A(struct z80 *cpu);
static inline void OUT_cC_r(struct z80 *cpu, uint8_t *r);
static inline void OUTI(struct z80 *cpu);
static inline void OTIR(struct z80 *cpu);
static inline void OUTD(struct z80 *cpu);
static inline void OTDR(struct z80 *cpu);

void LD_r_r(uint8_t *r1, uint8_t *r2)
{
	*r1 = *r2;
	clock_consume(4);
}

void LD_r_n(struct z80 *cpu, uint8_t *r)
{
	*r = memory_readb(cpu->bus_id, cpu->PC++);
	clock_consume(7);
}

void LD_r_cHL(struct z80 *cpu, uint8_t *r)
{
	*r = memory_readb(cpu->bus_id, cpu->HL);
	clock_consume(7);
}

void LD_r_cIXYpd(struct z80 *cpu, uint8_t *r, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	*r = memory_readb(cpu->bus_id, address);
	clock_consume(19);
}

void LD_cHL_r(struct z80 *cpu, uint8_t *r)
{
	memory_writeb(cpu->bus_id, *r, cpu->HL);
	clock_consume(7);
}

void LD_cIXYpd_r(struct z80 *cpu, uint8_t *r, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	memory_writeb(cpu->bus_id, *r, address);
	clock_consume(19);
}

void LD_cHL_n(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->PC++);
	memory_writeb(cpu->bus_id, b, cpu->HL);
	clock_consume(10);
}

void LD_cIXYpd_n(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	memory_writeb(cpu->bus_id, n, address);
	clock_consume(19);
}

void LD_A_cBC(struct z80 *cpu)
{
	cpu->A = memory_readb(cpu->bus_id, cpu->BC);
	clock_consume(7);
}

void LD_A_cDE(struct z80 *cpu)
{
	cpu->A = memory_readb(cpu->bus_id, cpu->DE);
	clock_consume(7);
}

void LD_A_cnn(struct z80 *cpu)
{
	uint16_t address = memory_readb(cpu->bus_id, cpu->PC++);
	address |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	cpu->A = memory_readb(cpu->bus_id, address);
	clock_consume(13);
}

void LD_cBC_A(struct z80 *cpu)
{
	memory_writeb(cpu->bus_id, cpu->A, cpu->BC);
	clock_consume(7);
}

void LD_cDE_A(struct z80 *cpu)
{
	memory_writeb(cpu->bus_id, cpu->A, cpu->DE);
	clock_consume(7);
}

void LD_cnn_A(struct z80 *cpu)
{
	uint16_t address = memory_readb(cpu->bus_id, cpu->PC++);
	address |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	memory_writeb(cpu->bus_id, cpu->A, address);
	clock_consume(13);
}

void LD_A_I(struct z80 *cpu)
{
	cpu->A = cpu->I;
	cpu->flags.S = ((int8_t)cpu->I < 0);
	cpu->flags.Z = (cpu->I == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = cpu->IFF2;
	cpu->flags.N = 0;
	clock_consume(9);
}

void LD_A_R(struct z80 *cpu)
{
	cpu->A = cpu->R;
	cpu->flags.S = ((int8_t)cpu->R < 0);
	cpu->flags.Z = (cpu->R == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = cpu->IFF2;
	cpu->flags.N = 0;
	clock_consume(9);
}

void LD_I_A(struct z80 *cpu)
{
	cpu->I = cpu->A;
	clock_consume(9);
}

void LD_R_A(struct z80 *cpu)
{
	cpu->R = cpu->A;
	clock_consume(9);
}

void LD_dd_nn(struct z80 *cpu, uint16_t *dd)
{
	uint16_t nn = memory_readb(cpu->bus_id, cpu->PC++);
	nn |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	*dd = nn;
	clock_consume(10);
}

void LD_IXY_nn(struct z80 *cpu, uint16_t *reg)
{
	uint16_t nn = memory_readb(cpu->bus_id, cpu->PC++);
	nn |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	*reg = nn;
	clock_consume(14);
}

void LD_HL_cnn(struct z80 *cpu)
{
	uint16_t address = memory_readb(cpu->bus_id, cpu->PC++);
	address |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	cpu->L = memory_readb(cpu->bus_id, address);
	cpu->H = memory_readb(cpu->bus_id, address + 1);
	clock_consume(16);
}

void LD_dd_cnn(struct z80 *cpu, uint16_t *dd)
{
	uint16_t address = memory_readb(cpu->bus_id, cpu->PC++);
	address |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	*dd = memory_readb(cpu->bus_id, address);
	*dd |= memory_readb(cpu->bus_id, address + 1) << 8;
	clock_consume(20);
}

void LD_IXY_cnn(struct z80 *cpu, uint16_t *reg)
{
	uint16_t address = memory_readb(cpu->bus_id, cpu->PC++);
	address |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	*reg = memory_readb(cpu->bus_id, address);
	*reg |= memory_readb(cpu->bus_id, address + 1) << 8;
	clock_consume(20);
}

void LD_cnn_HL(struct z80 *cpu)
{
	uint16_t nn = memory_readb(cpu->bus_id, cpu->PC++);
	nn |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	memory_writeb(cpu->bus_id, cpu->HL, nn);
	memory_writeb(cpu->bus_id, cpu->HL >> 8, nn + 1);
	clock_consume(16);
}

void LD_cnn_dd(struct z80 *cpu, uint16_t *dd)
{
	uint16_t nn = memory_readb(cpu->bus_id, cpu->PC++);
	nn |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	memory_writeb(cpu->bus_id, *dd, nn);
	memory_writeb(cpu->bus_id, *dd >> 8, nn + 1);
	clock_consume(20);
}

void LD_cnn_IXY(struct z80 *cpu, uint16_t *reg)
{
	uint16_t nn = memory_readb(cpu->bus_id, cpu->PC++);
	nn |= memory_readb(cpu->bus_id, cpu->PC++) << 8;
	memory_writeb(cpu->bus_id, *reg, nn);
	memory_writeb(cpu->bus_id, *reg >> 8, nn + 1);
	clock_consume(20);
}

void LD_SP_HL(struct z80 *cpu)
{
	cpu->SP = cpu->HL;
	clock_consume(6);
}

void LD_SP_IXY(struct z80 *cpu, uint16_t *reg)
{
	cpu->SP = *reg;
	clock_consume(10);
}

void PUSH_qq(struct z80 *cpu, uint16_t *qq)
{
	memory_writeb(cpu->bus_id, *qq >> 8, --cpu->SP);
	memory_writeb(cpu->bus_id, *qq, --cpu->SP);
	clock_consume(11);
}

void PUSH_IXY(struct z80 *cpu, uint16_t *reg)
{
	memory_writeb(cpu->bus_id, *reg >> 8, --cpu->SP);
	memory_writeb(cpu->bus_id, *reg, --cpu->SP);
	clock_consume(15);
}

void POP_qq(struct z80 *cpu, uint16_t *qq)
{
	*qq = memory_readb(cpu->bus_id, cpu->SP++);
	*qq |= memory_readb(cpu->bus_id, cpu->SP++) << 8;
	clock_consume(10);
}

void POP_IXY(struct z80 *cpu, uint16_t *reg)
{
	*reg = memory_readb(cpu->bus_id, cpu->SP++);
	*reg |= memory_readb(cpu->bus_id, cpu->SP++) << 8;
	clock_consume(14);
}

void EX_DE_HL(struct z80 *cpu)
{
	uint16_t DE = cpu->DE;
	cpu->DE = cpu->HL;
	cpu->HL = DE;
	clock_consume(4);
}

void EX_AF_A2F2(struct z80 *cpu)
{
	uint16_t AF = cpu->AF;
	cpu->AF = cpu->A2F2;
	cpu->A2F2 = AF;
	clock_consume(4);
}

void EXX(struct z80 *cpu)
{
	uint16_t BC = cpu->BC;
	uint16_t DE = cpu->DE;
	uint16_t HL = cpu->HL;
	cpu->BC = cpu->B2C2;
	cpu->DE = cpu->D2E2;
	cpu->HL = cpu->H2L2;
	cpu->B2C2 = BC;
	cpu->D2E2 = DE;
	cpu->H2L2 = HL;
	clock_consume(4);
}

void EX_cSP_HL(struct z80 *cpu)
{
	uint8_t b1 = memory_readb(cpu->bus_id, cpu->SP);
	uint8_t b2 = memory_readb(cpu->bus_id, cpu->SP + 1);
	memory_writeb(cpu->bus_id, cpu->L, cpu->SP);
	memory_writeb(cpu->bus_id, cpu->H, cpu->SP + 1);
	cpu->HL = b1 | (b2 << 8);
	clock_consume(19);
}

void EX_cSP_IXY(struct z80 *cpu, uint16_t *reg)
{
	uint8_t b1 = memory_readb(cpu->bus_id, cpu->SP);
	uint8_t b2 = memory_readb(cpu->bus_id, cpu->SP + 1);
	memory_writeb(cpu->bus_id, *reg, cpu->SP);
	memory_writeb(cpu->bus_id, *reg >> 8, cpu->SP + 1);
	*reg = b1 | (b2 << 8);
	clock_consume(23);
}

void LDI(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL++);
	memory_writeb(cpu->bus_id, b, cpu->DE++);
	cpu->BC--;
	cpu->flags.H = 0;
	cpu->flags.PV = (cpu->BC != 0);
	cpu->flags.N = 0;
	clock_consume(16);
}

void LDIR(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL++);
	memory_writeb(cpu->bus_id, b, cpu->DE++);
	if (--cpu->BC != 0) {
		cpu->PC -= 2;
		clock_consume(5);
	}
	cpu->flags.H = 0;
	cpu->flags.PV = (cpu->BC != 0);
	cpu->flags.N = 0;
	clock_consume(16);
}

void LDD(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL--);
	memory_writeb(cpu->bus_id, b, cpu->DE--);
	cpu->BC--;
	cpu->flags.H = 0;
	cpu->flags.PV = (cpu->BC != 0);
	cpu->flags.N = 0;
	clock_consume(16);
}

void LDDR(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL--);
	memory_writeb(cpu->bus_id, b, cpu->DE--);
	if (--cpu->BC != 0) {
		cpu->PC -= 2;
		clock_consume(5);
	}
	cpu->flags.H = 0;
	cpu->flags.PV = (cpu->BC != 0);
	cpu->flags.N = 0;
	clock_consume(16);
}

void CPI(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL++);
	int8_t result = cpu->A - b;
	cpu->BC--;
	cpu->flags.S = (result < 0);
	cpu->flags.Z = (result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (b & 0x0F) < 0);
	cpu->flags.PV = (cpu->BC == 0);
	cpu->flags.N = 1;
	clock_consume(16);
}

void CPIR(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL++);
	int8_t result = cpu->A - b;
	cpu->BC--;
	if ((cpu->BC != 0) && (result != 0)) {
		cpu->PC -= 2;
		clock_consume(5);
	}
	cpu->flags.S = (result < 0);
	cpu->flags.Z = (result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (b & 0x0F) < 0);
	cpu->flags.PV = (cpu->BC == 0);
	cpu->flags.N = 1;
	clock_consume(16);
}

void CPD(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL--);
	int8_t result = cpu->A - b;
	cpu->BC--;
	cpu->flags.S = (result < 0);
	cpu->flags.Z = (result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (b & 0x0F) < 0);
	cpu->flags.PV = (cpu->BC == 0);
	cpu->flags.N = 1;
	clock_consume(16);
}

void CPDR(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL--);
	int8_t result = cpu->A - b;
	cpu->BC--;
	if ((cpu->BC != 0) && (result != 0)) {
		cpu->PC -= 2;
		clock_consume(5);
	}
	cpu->flags.S = (result < 0);
	cpu->flags.Z = (result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (b & 0x0F) < 0);
	cpu->flags.PV = (cpu->BC == 0);
	cpu->flags.N = 1;
	clock_consume(16);
}

void ADD_A_r(struct z80 *cpu, uint8_t *r)
{
	int16_t result = cpu->A + *r;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) + (*r & 0x0F) > 0x0F);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 0;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(4);
}

void ADD_A_n(struct z80 *cpu)
{
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	int16_t result = cpu->A + n;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) + (n & 0x0F) > 0x0F);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 0;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(7);
}

void ADD_A_cHL(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL);
	int16_t result = cpu->A + b;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) + (b & 0x0F) > 0x0F);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 0;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(7);
}

void ADD_A_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t b = memory_readb(cpu->bus_id, address);
	int16_t result = cpu->A + b;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) + (b & 0x0F) > 0x0F);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 0;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(19);
}

void ADC_A_r(struct z80 *cpu, uint8_t *r)
{
	int16_t result = cpu->A + *r + cpu->flags.C;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) + (*r & 0x0F) + cpu->flags.C > 0x0F);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 0;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(4);
}

void ADC_A_n(struct z80 *cpu)
{
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	int16_t result = cpu->A + n + cpu->flags.C;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) + (n & 0x0F) + cpu->flags.C > 0x0F);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 0;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(7);
}

void ADC_A_cHL(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL);
	int16_t result = cpu->A + b + cpu->flags.C;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) + (b & 0x0F) + cpu->flags.C > 0x0F);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 0;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(7);
}

void ADC_A_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t b = memory_readb(cpu->bus_id, address);
	int16_t result = cpu->A + b + cpu->flags.C;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) + (b & 0x0F) + cpu->flags.C > 0x0F);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 0;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(19);
}

void SUB_A_r(struct z80 *cpu, uint8_t *r)
{
	int16_t result = cpu->A - *r;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (*r & 0x0F) < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(4);
}

void SUB_A_n(struct z80 *cpu)
{
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	int16_t result = cpu->A - n;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (n & 0x0F) < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(7);
}

void SUB_A_cHL(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL);
	int16_t result = cpu->A - memory_readb(cpu->bus_id, cpu->HL);
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (b & 0x0F) < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(7);
}

void SUB_A_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t b = memory_readb(cpu->bus_id, address);
	int16_t result = cpu->A - b;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (b & 0x0F) < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(19);
}

void SBC_A_r(struct z80 *cpu, uint8_t *r)
{
	int16_t result = cpu->A - *r - cpu->flags.C;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (*r & 0x0F) - cpu->flags.C < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(4);
}

void SBC_A_n(struct z80 *cpu)
{
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	int16_t result = cpu->A - n - cpu->flags.C;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (n & 0x0F) - cpu->flags.C < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(7);
}

void SBC_A_cHL(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL);
	int16_t result = cpu->A - b - cpu->flags.C;
	cpu->flags.S = (result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - b - cpu->flags.C < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(7);
}

void SBC_A_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t b = memory_readb(cpu->bus_id, address);
	int16_t result = cpu->A - b - cpu->flags.C;
	cpu->flags.S = (result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - b - cpu->flags.C < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 8;
	cpu->A = result;
	clock_consume(19);
}

void AND_r(struct z80 *cpu, uint8_t *r)
{
	cpu->A &= *r;
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 1;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	cpu->flags.C = 0;
	clock_consume(4);
}

void AND_n(struct z80 *cpu)
{
	cpu->A &= memory_readb(cpu->bus_id, cpu->PC++);
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 1;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	cpu->flags.C = 0;
	clock_consume(7);
}

void AND_cHL(struct z80 *cpu)
{
	cpu->A &= memory_readb(cpu->bus_id, cpu->HL);
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 1;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	cpu->flags.C = 0;
	clock_consume(7);
}

void AND_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	cpu->A &= memory_readb(cpu->bus_id, *reg + d);
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 1;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	cpu->flags.C = 0;
	clock_consume(19);
}

void OR_r(struct z80 *cpu, uint8_t *r)
{
	cpu->A |= *r;
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	cpu->flags.C = 0;
	clock_consume(4);
}

void OR_n(struct z80 *cpu)
{
	cpu->A |= memory_readb(cpu->bus_id, cpu->PC++);
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	cpu->flags.C = 0;
	clock_consume(7);
}

void OR_cHL(struct z80 *cpu)
{
	cpu->A |= memory_readb(cpu->bus_id, cpu->HL);
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	cpu->flags.C = 0;
	clock_consume(7);
}

void OR_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	cpu->A |= memory_readb(cpu->bus_id, *reg + d);
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	cpu->flags.C = 0;
	clock_consume(19);
}

void XOR_r(struct z80 *cpu, uint8_t *r)
{
	cpu->A ^= *r;
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	cpu->flags.C = 0;
	clock_consume(4);
}

void XOR_n(struct z80 *cpu)
{
	cpu->A ^= memory_readb(cpu->bus_id, cpu->PC++);
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	cpu->flags.C = 0;
	clock_consume(7);
}

void XOR_cHL(struct z80 *cpu)
{
	cpu->A ^= memory_readb(cpu->bus_id, cpu->HL);
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	cpu->flags.C = 0;
	clock_consume(7);
}

void XOR_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	cpu->A ^= memory_readb(cpu->bus_id,  *reg + d);
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	cpu->flags.C = 0;
	clock_consume(19);
}

void CP_r(struct z80 *cpu, uint8_t *r)
{
	int16_t result = cpu->A - *r;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (*r & 0x0F) < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 8;
	clock_consume(4);
}

void CP_n(struct z80 *cpu)
{
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	int16_t result = cpu->A - n;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (n & 0x0F) < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 8;
	clock_consume(7);
}

void CP_cHL(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL);
	int16_t result = cpu->A - b;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (b & 0x0F) < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 8;
	clock_consume(7);
}

void CP_IXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t b = memory_readb(cpu->bus_id, address);
	int16_t result = cpu->A - b;
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = ((uint8_t)result == 0);
	cpu->flags.H = ((cpu->A & 0x0F) - (b & 0x0F) < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 8;
	clock_consume(19);
}

void INC_r(struct z80 *cpu, uint8_t *r)
{
	cpu->flags.S = ((int8_t)(*r + 1) < 0);
	cpu->flags.Z = (*r == 0xFF);
	cpu->flags.H = ((*r & 0x0F) == 0x0F);
	cpu->flags.PV = (*r == 0x7F);
	cpu->flags.N = 0;
	(*r)++;
	clock_consume(4);
}

void INC_cHL(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL);
	cpu->flags.S = ((int8_t)(b + 1) < 0);
	cpu->flags.Z = (b == 0xFF);
	cpu->flags.H = ((b & 0x0F) == 0x0F);
	cpu->flags.PV = (b == 0x7F);
	cpu->flags.N = 0;
	memory_writeb(cpu->bus_id, b + 1, cpu->HL);
	clock_consume(11);
}

void INC_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t b = memory_readb(cpu->bus_id, address);
	cpu->flags.S = ((int8_t)(b + 1) < 0);
	cpu->flags.Z = (b == 0xFF);
	cpu->flags.H = ((b & 0x0F) == 0x0F);
	cpu->flags.PV = (b == 0x7F);
	cpu->flags.N = 0;
	memory_writeb(cpu->bus_id, b + 1, address);
	clock_consume(23);
}

void DEC_r(struct z80 *cpu, uint8_t *r)
{
	(*r)--;
	cpu->flags.S = ((int8_t)*r < 0);
	cpu->flags.Z = (*r == 0);
	cpu->flags.H = ((*r & 0x0F) == 0x0F);
	cpu->flags.PV = (*r == 0x7F);
	cpu->flags.N = 1;
	clock_consume(4);
}

void DEC_cHL(struct z80 *cpu)
{
	int8_t result = memory_readb(cpu->bus_id, cpu->HL) - 1;
	memory_writeb(cpu->bus_id, result, cpu->HL);
	cpu->flags.S = (result < 0);
	cpu->flags.Z = (result == 0);
	cpu->flags.H = ((result & 0x0F) == 0x0F);
	cpu->flags.PV = (result == 0x7F);
	cpu->flags.N = 1;
	clock_consume(11);
}

void DEC_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t b = memory_readb(cpu->bus_id, address);
	int8_t result = b - 1;
	memory_writeb(cpu->bus_id, result, address);
	cpu->flags.S = (result < 0);
	cpu->flags.Z = (result == 0);
	cpu->flags.H = ((b & 0x0F) - 1 == 0x0F);
	cpu->flags.PV = (b == 0x80);
	cpu->flags.N = 0;
	clock_consume(23);
}

void DAA(struct z80 *cpu)
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
	cpu->A = a & 0xFF;

	/* Set flags */
	cpu->flags.S = ((cpu->A & 0x80) != 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 0;

	/* Set carry flag if needed */
	if (a & 0x100)
		cpu->flags.C = 1;

	/* Consume cycles */
	clock_consume(4);
}

void CPL(struct z80 *cpu)
{
	cpu->A = ~cpu->A;
	cpu->flags.H = 1;
	cpu->flags.N = 1;
	clock_consume(4);
}

void NEG(struct z80 *cpu)
{
	int8_t result = -cpu->A;
	cpu->flags.S = (result < 0);
	cpu->flags.Z = (result == 0);
	cpu->flags.H = ((result & 0x0F) < 0);
	cpu->flags.PV = (cpu->A != 0x80);
	cpu->flags.N = 1;
	cpu->flags.C = (cpu->A != 0);
	cpu->A = result;
	clock_consume(8);
}

void CCF(struct z80 *cpu)
{
	cpu->flags.H = cpu->flags.C;
	cpu->flags.N = 0;
	cpu->flags.C = !cpu->flags.C;
	clock_consume(4);
}

void SCF(struct z80 *cpu)
{
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = 1;
	clock_consume(4);
}

void NOP(struct z80 *UNUSED(cpu))
{
	clock_consume(4);
}

void HALT(struct z80 *cpu)
{
	cpu->halted = true;
	cpu->PC--;
	clock_consume(4);
}

void DI(struct z80 *cpu)
{
	cpu->IFF1 = false;
	cpu->IFF2 = false;
	clock_consume(4);
}

void EI(struct z80 *cpu)
{
	cpu->IFF1 = true;
	cpu->IFF2 = true;
	cpu->irq_delay = true;
	clock_consume(4);
}

void IM_0(struct z80 *cpu)
{
	LOG_W("IM 0: unsupported interrupt mode!\n");
	cpu->interrupt_mode = 0;
	clock_consume(8);
}

void IM_1(struct z80 *cpu)
{
	cpu->interrupt_mode = 1;
	clock_consume(8);
}

void IM_2(struct z80 *cpu)
{
	LOG_W("IM 2: unsupported interrupt mode!\n");
	cpu->interrupt_mode = 2;
	clock_consume(8);
}

void ADD_HL_ss(struct z80 *cpu, uint16_t *ss)
{
	uint32_t result = cpu->HL + *ss;
	cpu->flags.H = ((cpu->HL & 0x0FFF) + (*ss & 0x0FFF) > 0x0FFF);
	cpu->flags.N = 0;
	cpu->flags.C = result >> 16;
	cpu->HL = result;
	clock_consume(11);
}

void ADC_HL_rr(struct z80 *cpu, uint16_t *rr)
{
	int32_t result = cpu->HL + *rr + cpu->flags.C;
	uint16_t h_result = (cpu->HL & 0x0FFF) + (*rr & 0x0FFF) + cpu->flags.C;
	cpu->flags.S = ((int16_t)result < 0);
	cpu->flags.Z = ((uint16_t)result == 0);
	cpu->flags.H =  (h_result > 0x0FFF);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 0;
	cpu->flags.C = result >> 16;
	cpu->HL = result;
	clock_consume(15);
}

void SBC_HL_rr(struct z80 *cpu, uint16_t *rr)
{
	int32_t result = cpu->HL - *rr - cpu->flags.C;
	int16_t h_result = (cpu->HL & 0x0FFF) - (*rr & 0x0FFF) - cpu->flags.C;
	cpu->flags.S = ((int16_t)result < 0);
	cpu->flags.Z = ((uint16_t)result == 0);
	cpu->flags.H = (h_result < 0);
	cpu->flags.PV = ((uint16_t)result > 0xFF);
	cpu->flags.N = 1;
	cpu->flags.C = result >> 16;
	cpu->HL = result;
	clock_consume(15);
}

void ADD_IXY_pp(struct z80 *cpu, uint16_t *rr, uint16_t *reg)
{
	uint32_t result = *reg + *rr;
	cpu->flags.H = ((*reg & 0x0FFF) + (*rr & 0x0FFF) > 0x0FFF);
	cpu->flags.N = 0;
	cpu->flags.C = result >> 16;
	*reg = result;
	clock_consume(15);
}

void INC_ss(uint16_t *ss)
{
	(*ss)++;
	clock_consume(6);
}

void DEC_ss(uint16_t *rr)
{
	(*rr)--;
	clock_consume(6);
}

void DEC_IXY(uint16_t *reg)
{
	(*reg)--;
	clock_consume(10);
}

void RLCA(struct z80 *cpu)
{
	cpu->flags.Z = 0;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((cpu->A & 0x80) != 0);
	cpu->A = (cpu->A << 1) | cpu->flags.C;
	clock_consume(4);
}

void RLA(struct z80 *cpu)
{
	int old_carry = cpu->flags.C;
	cpu->flags.Z = 0;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((cpu->A & 0x80) != 0);
	cpu->A = (cpu->A << 1) | old_carry;
	clock_consume(4);
}

void RRCA(struct z80 *cpu)
{
	cpu->flags.Z = 0;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((cpu->A & 0x01) != 0);
	cpu->A = (cpu->A >> 1) | (cpu->flags.C << 7);
	clock_consume(4);
}

void RRA(struct z80 *cpu)
{
	int old_carry = cpu->flags.C;
	cpu->flags.Z = 0;
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((cpu->A & 0x01) != 0);
	cpu->A = (cpu->A >> 1) | (old_carry << 7);
	clock_consume(4);
}

void RLC_r(struct z80 *cpu, uint8_t *r)
{
	uint8_t b = *r;
	b = (*r << 1) | (*r >> 7);
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((*r & 0x80) != 0);
	*r = b;
	clock_consume(8);
}

void RLC_cHL(struct z80 *cpu)
{
	uint8_t cHL = memory_readb(cpu->bus_id, cpu->HL);
	uint8_t b = ((cHL << 1) | (cHL >> 7));
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((cHL & 0x80) != 0);
	memory_writeb(cpu->bus_id, b, cpu->HL);
	clock_consume(15);
}

void RLC_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t cIXYpd = memory_readb(cpu->bus_id, address);
	uint8_t b = ((cIXYpd << 1) | (cIXYpd >> 7));
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((cIXYpd & 0x80) != 0);
	memory_writeb(cpu->bus_id, (b << 1) | cpu->flags.C, address);
	clock_consume(23);
}

void RL_r(struct z80 *cpu, uint8_t *r)
{
	int old_carry = cpu->flags.C;
	uint8_t b = (*r << 1) | old_carry;
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((*r & 0x80) != 0);
	*r = (*r << 1) | old_carry;
	clock_consume(8);
}

void RL_cHL(struct z80 *cpu)
{
	uint8_t cHL = memory_readb(cpu->bus_id, cpu->HL);
	int old_carry = cpu->flags.C;
	uint8_t b = (cHL << 1) | old_carry;
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((cHL & 0x80) != 0);
	memory_writeb(cpu->bus_id, b, cpu->HL);
	clock_consume(15);
}

void RL_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t cIXYpd = memory_readb(cpu->bus_id, address);
	int old_carry = cpu->flags.C;
	uint8_t b = (cIXYpd << 1) | old_carry;
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((cIXYpd & 0x80) != 0);
	memory_writeb(cpu->bus_id, b, address);
	clock_consume(23);
}

void RRC_r(struct z80 *cpu, uint8_t *r)
{
	uint8_t result = (*r >> 1) | (cpu->flags.C << 7);
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = (result == 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((*r & 0x01) != 0);
	*r = result;
	clock_consume(8);
}

void RRC_cHL(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL);
	uint8_t result = (b >> 1) | (cpu->flags.C << 7);
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = (result == 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((b & 0x01) != 0);
	memory_writeb(cpu->bus_id, result, cpu->HL);
	clock_consume(15);
}

void RRC_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t b = memory_readb(cpu->bus_id, address);
	uint8_t result = (b >> 1) | (cpu->flags.C << 7);
	cpu->flags.S = ((int8_t)result < 0);
	cpu->flags.Z = (result == 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((b & 0x01) != 0);
	memory_writeb(cpu->bus_id, result, address);
	clock_consume(23);
}

void RR_r(struct z80 *cpu, uint8_t *r)
{
	int old_carry = cpu->flags.C;
	uint8_t b = (*r >> 1) | (old_carry << 7);
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((*r & 0x01) != 0);
	*r = b;
	clock_consume(8);
}

void RR_cHL(struct z80 *cpu)
{
	uint8_t cHL = memory_readb(cpu->bus_id, cpu->HL);
	int old_carry = cpu->flags.C;
	uint8_t b = (cHL >> 1) | (old_carry << 7);
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((cHL & 0x01) != 0);
	memory_writeb(cpu->bus_id, b, cpu->HL);
	clock_consume(15);
}

void RR_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t cIXYpd = memory_readb(cpu->bus_id, address);
	int old_carry = cpu->flags.C;
	uint8_t b = (cIXYpd >> 1) | (old_carry << 7);
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.N = 0;
	cpu->flags.C = ((cIXYpd & 0x01) != 0);
	memory_writeb(cpu->bus_id, b, address);
	clock_consume(23);
}

void SLA_r(struct z80 *cpu, uint8_t *r)
{
	uint8_t b = *r << 1;
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(b);
	cpu->flags.N = 0;
	cpu->flags.C = ((*r & 0x80) != 0);
	*r = b;
	clock_consume(8);
}

void SLA_cHL(struct z80 *cpu)
{
	uint8_t cHL = memory_readb(cpu->bus_id, cpu->HL);
	uint8_t b = cHL << 1;
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(b);
	cpu->flags.N = 0;
	cpu->flags.C = ((cHL & 0x80) != 0);
	memory_writeb(cpu->bus_id, b, cpu->HL);
	clock_consume(15);
}

void SLA_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t cIXYpd = memory_readb(cpu->bus_id, address);
	uint8_t b = cIXYpd << 1;
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = ((b << 1) == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(b);
	cpu->flags.N = 0;
	cpu->flags.C = ((cIXYpd & 0x80) != 0);
	memory_writeb(cpu->bus_id, b, address);
	clock_consume(23);
}

void SRA_r(struct z80 *cpu, uint8_t *r)
{
	uint8_t b = (*r >> 1) | (*r & 0x80);
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(b);
	cpu->flags.N = 0;
	cpu->flags.C = ((*r & 0x01) != 0);
	*r = b;
	clock_consume(8);
}

void SRA_cHL(struct z80 *cpu)
{
	uint8_t cHL = memory_readb(cpu->bus_id, cpu->HL);
	uint8_t b = (cHL >> 1) | (cHL & 0x80);
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(b);
	cpu->flags.N = 0;
	cpu->flags.C = ((cHL & 0x01) != 0);
	memory_writeb(cpu->bus_id, b, cpu->HL);
	clock_consume(15);
}

void SRA_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t cIXYpd = memory_readb(cpu->bus_id, address);
	uint8_t b = (cIXYpd >> 1) | (cIXYpd & 0x80);
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(b);
	cpu->flags.N = 0;
	cpu->flags.C = ((cIXYpd & 0x01) != 0);
	memory_writeb(cpu->bus_id, b, address);
	clock_consume(23);
}

void SL1_r(struct z80 *cpu, uint8_t *r)
{
	uint8_t b = (*r << 1) | 0x01;
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = 0;
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(b);
	cpu->flags.N = 0;
	cpu->flags.C = ((*r & 0x80) != 0);
	*r = b;
	clock_consume(8);
}

void SL1_cHL(struct z80 *cpu)
{
	uint8_t cHL = memory_readb(cpu->bus_id, cpu->HL);
	uint8_t b = (cHL << 1) | 0x01;
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = 0;
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(b);
	cpu->flags.N = 0;
	cpu->flags.C = ((cHL & 0x80) != 0);
	memory_writeb(cpu->bus_id, b, cpu->HL);
	clock_consume(15);
}

void SL1_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t cIXYpd = memory_readb(cpu->bus_id, address);
	uint8_t b = (cIXYpd << 1) | 0x01;
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = 0;
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(b);
	cpu->flags.N = 0;
	cpu->flags.C = ((cIXYpd & 0x80) != 0);
	memory_writeb(cpu->bus_id, b, address);
	clock_consume(23);
}

void SRL_r(struct z80 *cpu, uint8_t *r)
{
	uint8_t b = *r >> 1;
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(b);
	cpu->flags.N = 0;
	cpu->flags.C = ((*r & 0x01) != 0);
	*r = b;
	clock_consume(8);
}

void SRL_cHL(struct z80 *cpu)
{
	uint8_t cHL = memory_readb(cpu->bus_id, cpu->HL);
	uint8_t b = cHL >> 1;
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(b);
	cpu->flags.N = 0;
	cpu->flags.C = ((cHL & 0x01) != 0);
	memory_writeb(cpu->bus_id, b, cpu->HL);
	clock_consume(15);
}

void SRL_cIXYpd(struct z80 *cpu, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t cIXYpd = memory_readb(cpu->bus_id, address);
	uint8_t b = cIXYpd >> 1;
	cpu->flags.S = ((int8_t)b < 0);
	cpu->flags.Z = (b == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(b);
	cpu->flags.N = 0;
	cpu->flags.C = ((cIXYpd & 0x01) != 0);
	memory_writeb(cpu->bus_id, b, address);
	clock_consume(23);
}

void RLD(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL);
	memory_writeb(cpu->bus_id, (b << 4) | (cpu->A & 0x0F), cpu->HL);
	cpu->A = (cpu->A & 0xF0) | (b >> 4);
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	clock_consume(18);
}

void RRD(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL);
	memory_writeb(cpu->bus_id, (b >> 4) | ((cpu->A & 0x0F) << 4), cpu->HL);
	cpu->A = (cpu->A & 0xF0) | (b & 0x0F);
	cpu->flags.S = ((int8_t)cpu->A < 0);
	cpu->flags.Z = (cpu->A == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = !bitops_parity(cpu->A);
	cpu->flags.N = 0;
	clock_consume(18);
}

void BIT_b_r(struct z80 *cpu, uint8_t b, uint8_t *r)
{
	cpu->flags.Z = ((*r & (1 << b)) == 0);
	cpu->flags.H = 1;
	cpu->flags.N = 0;
	clock_consume(8);
}

void BIT_b_cHL(struct z80 *cpu, uint8_t b)
{
	cpu->flags.Z = ((memory_readb(cpu->bus_id, cpu->HL) & (1 << b)) == 0);
	cpu->flags.H = 1;
	cpu->flags.N = 0;
	clock_consume(12);
}

void BIT_b_cIXYpd(struct z80 *cpu, uint8_t b, uint16_t *reg)
{
	int8_t d;
	uint16_t address;
	uint8_t cIXYpd;
	d = memory_readb(cpu->bus_id, cpu->PC++);
	address = *reg + d;
	cIXYpd = memory_readb(cpu->bus_id, address);
	cpu->flags.Z = ((cIXYpd & (1 << b)) == 0);
	cpu->flags.H = 1;
	cpu->flags.N = 0;
	clock_consume(20);
}

void SET_b_r(uint8_t b, uint8_t *r)
{
	*r |= (1 << b);
	clock_consume(8);
}

void SET_b_cHL(struct z80 *cpu, uint8_t b)
{
	uint8_t cHL = memory_readb(cpu->bus_id, cpu->HL);
	memory_writeb(cpu->bus_id, cHL | (1 << b), cpu->HL);
	clock_consume(15);
}

void SET_b_cIXYpd(struct z80 *cpu, uint8_t b, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t cIXYpd = memory_readb(cpu->bus_id, address);
	memory_writeb(cpu->bus_id, cIXYpd | (1 << b), address);
	clock_consume(23);
}

void RES_b_r(uint8_t b, uint8_t *r)
{
	*r &= ~(1 << b);
	clock_consume(8);
}

void RES_b_cHL(struct z80 *cpu, uint8_t b)
{
	uint8_t cHL = memory_readb(cpu->bus_id, cpu->HL);
	memory_writeb(cpu->bus_id, cHL & ~(1 << b), cpu->HL);
	clock_consume(15);
}

void RES_b_cIXYpd(struct z80 *cpu, uint8_t b, uint16_t *reg)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	uint16_t address = *reg + d;
	uint8_t cIXYpd = memory_readb(cpu->bus_id, address);
	memory_writeb(cpu->bus_id, cIXYpd & ~(1 << b), address);
	clock_consume(23);
}

void JP_nn(struct z80 *cpu)
{
	uint8_t n1 = memory_readb(cpu->bus_id, cpu->PC++);
	uint8_t n2 = memory_readb(cpu->bus_id, cpu->PC++);
	cpu->PC = n1 | (n2 << 8);
	clock_consume(10);
}

void JP_cc_nn(struct z80 *cpu, bool condition)
{
	uint8_t n1 = memory_readb(cpu->bus_id, cpu->PC++);
	uint8_t n2 = memory_readb(cpu->bus_id, cpu->PC++);
	if (condition)
		cpu->PC = n1 | (n2 << 8);
	clock_consume(10);
}

void JR_e(struct z80 *cpu)
{
	int8_t e = memory_readb(cpu->bus_id, cpu->PC++);
	cpu->PC += e;
	clock_consume(12);
}

void JR_cc_e(struct z80 *cpu, bool condition)
{
	int8_t d = memory_readb(cpu->bus_id, cpu->PC++);
	if (condition) {
		cpu->PC += d;
		clock_consume(5);
	}
	clock_consume(7);
}

void JP_HL(struct z80 *cpu)
{
	cpu->PC = cpu->HL;
	clock_consume(4);
}

void JP_IXY(struct z80 *cpu, uint16_t *reg)
{
	cpu->PC = *reg;
	clock_consume(8);
}

void DJNZ_e(struct z80 *cpu)
{
	int8_t e = memory_readb(cpu->bus_id, cpu->PC++);
	if (--cpu->B != 0) {
		cpu->PC += e;
		clock_consume(5);
	}
	clock_consume(8);
}

void CALL_nn(struct z80 *cpu)
{
	uint8_t n1 = memory_readb(cpu->bus_id, cpu->PC++);
	uint8_t n2 = memory_readb(cpu->bus_id, cpu->PC++);
	memory_writeb(cpu->bus_id, cpu->PC >> 8, --cpu->SP);
	memory_writeb(cpu->bus_id, cpu->PC, --cpu->SP);
	cpu->PC = n1 | (n2 << 8);
	clock_consume(17);
}

void CALL_cc_nn(struct z80 *cpu, bool condition)
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

void RET(struct z80 *cpu)
{
	cpu->PC = memory_readb(cpu->bus_id, cpu->SP++);
	cpu->PC |= memory_readb(cpu->bus_id, cpu->SP++) << 8;
	clock_consume(10);
}

void RET_cc(struct z80 *cpu, bool condition)
{
	if (condition) {
		cpu->PC = memory_readb(cpu->bus_id, cpu->SP++);
		cpu->PC |= memory_readb(cpu->bus_id, cpu->SP++) << 8;
		clock_consume(6);
	}
	clock_consume(5);
}

void RETI(struct z80 *cpu)
{
	cpu->PC = memory_readb(cpu->bus_id, cpu->SP++);
	cpu->PC |= memory_readb(cpu->bus_id, cpu->SP++) << 8;
	cpu->IFF1 = cpu->IFF2;
	clock_consume(14);
}

void RETN(struct z80 *cpu)
{
	cpu->PC = memory_readb(cpu->bus_id, cpu->SP++);
	cpu->PC |= memory_readb(cpu->bus_id, cpu->SP++) << 8;
	cpu->IFF1 = cpu->IFF2;
	clock_consume(14);
}

void RST_p(struct z80 *cpu, uint8_t p)
{
	memory_writeb(cpu->bus_id, cpu->PC >> 8, --cpu->SP);
	memory_writeb(cpu->bus_id, cpu->PC, --cpu->SP);
	cpu->PC = p;
	clock_consume(11);
}

void IN_A_cn(struct z80 *cpu)
{
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	cpu->A = port_read(n);
	clock_consume(11);
}

void IN_r_cC(struct z80 *cpu, uint8_t *r)
{
	*r = port_read(cpu->C);
	cpu->flags.S = ((int8_t)*r < 0);
	cpu->flags.Z = (*r == 0);
	cpu->flags.H = 0;
	cpu->flags.PV = bitops_parity(*r);
	cpu->flags.N = 0;
	clock_consume(12);
}

void INI(struct z80 *cpu)
{
	uint8_t b = port_read(cpu->C);
	memory_writeb(cpu->bus_id, b, cpu->HL++);
	cpu->B--;
	cpu->flags.Z = (cpu->B == 0);
	cpu->flags.N = 1;
	clock_consume(16);
}

void INIR(struct z80 *cpu)
{
	uint8_t b = port_read(cpu->C);
	memory_writeb(cpu->bus_id, b, cpu->HL++);
	if (--cpu->B != 0) {
		cpu->PC -= 2;
		clock_consume(5);
	}
	cpu->flags.Z = (cpu->B == 0);
	cpu->flags.N = 1;
	clock_consume(16);
}

void IND(struct z80 *cpu)
{
	uint8_t b = port_read(cpu->C);
	memory_writeb(cpu->bus_id, b, cpu->HL--);
	cpu->B--;
	cpu->flags.Z = (cpu->B == 0);
	cpu->flags.N = 1;
	clock_consume(16);
}

void INDR(struct z80 *cpu)
{
	uint8_t b = port_read(cpu->C);
	memory_writeb(cpu->bus_id, b, cpu->HL--);
	if (--cpu->B != 0) {
		cpu->PC -= 2;
		clock_consume(5);
	}
	cpu->flags.Z = (cpu->B == 0);
	cpu->flags.N = 1;
	clock_consume(16);
}

void OUT_cn_A(struct z80 *cpu)
{
	uint8_t n = memory_readb(cpu->bus_id, cpu->PC++);
	port_write(cpu->A, n);
	clock_consume(11);
}

void OUT_cC_r(struct z80 *cpu, uint8_t *r)
{
	port_write(*r, cpu->C);
	clock_consume(12);
}

void OUTI(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL++);
	port_write(b, cpu->C);
	cpu->B--;
	cpu->flags.Z = (cpu->B == 0);
	cpu->flags.N = 1;
	clock_consume(16);
}

void OTIR(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL++);
	port_write(b, cpu->C);
	if (--cpu->B != 0) {
		cpu->PC -= 2;
		clock_consume(5);
	}
	cpu->flags.Z = (cpu->B == 0);
	cpu->flags.N = 1;
	clock_consume(16);
}

void OUTD(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL--);
	port_write(b, cpu->C);
	cpu->B--;
	cpu->flags.Z = (cpu->B == 0);
	cpu->flags.N = 1;
	clock_consume(16);
}

void OTDR(struct z80 *cpu)
{
	uint8_t b = memory_readb(cpu->bus_id, cpu->HL--);
	port_write(b, cpu->C);
	if (--cpu->B != 0) {
		cpu->PC -= 2;
		clock_consume(5);
	}
	cpu->flags.Z = (cpu->B == 0);
	cpu->flags.N = 1;
	clock_consume(16);
}

bool z80_handle_irq(struct z80 *cpu)
{
	/* Reset IRQ delay and exit if needed */
	if (cpu->irq_delay) {
		cpu->irq_delay = false;
		return false;
	}

	/* Leave if no IRQ is pending */
	if (!cpu->irq_pending)
		return false;

	/* Any interrupt should resume CPU */
	if (cpu->halted) {
		cpu->PC++;
		cpu->halted = false;
	}

	/* Check if interrupts are enabled */
	if (!cpu->IFF1)
		return false;

	/* Clear flip-flops */
	cpu->IFF1 = 0;
	cpu->IFF2 = 0;

	/* Push PC on stack */
	memory_writeb(cpu->bus_id, cpu->PC >> 8, --cpu->SP);
	memory_writeb(cpu->bus_id, cpu->PC, --cpu->SP);

	/* Jump to IRQ address */
	cpu->PC = IRQ_VECTOR;

	/* Interrupt handler should consume 20 cycles */
	clock_consume(20);

	/* Reset IRQ pending flag */
	cpu->irq_pending = false;
	return true;
}

bool z80_handle_nmi(struct z80 *cpu)
{
	/* Leave if no NMI is pending */
	if (!cpu->nmi_pending)
		return false;

	/* Push PC on stack */
	memory_writeb(cpu->bus_id, cpu->PC >> 8, --cpu->SP);
	memory_writeb(cpu->bus_id, cpu->PC, --cpu->SP);

	/* Jump to NMI address */
	cpu->PC = NMI_VECTOR;

	/* Interrupt handler should consume 20 cycles */
	clock_consume(20);

	/* Reset NMI pending flag */
	cpu->nmi_pending = false;
	return true;
}

void z80_tick(struct z80 *cpu)
{
	uint8_t opcode;

	/* Check for interrupt requests (IRQ or NMI) */
	if (z80_handle_irq(cpu) || z80_handle_nmi(cpu))
		return;

	/* Fetch opcode */
	opcode = memory_readb(cpu->bus_id, cpu->PC++);

	/* Execute opcode */
	switch (opcode) {
	case 0x00:
		NOP(cpu);
		break;
	case 0x01:
		LD_dd_nn(cpu, &cpu->BC);
		break;
	case 0x02:
		LD_cBC_A(cpu);
		break;
	case 0x03:
		INC_ss(&cpu->BC);
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
		EX_AF_A2F2(cpu);
		break;
	case 0x09:
		ADD_HL_ss(cpu, &cpu->BC);
		break;
	case 0x0A:
		LD_A_cBC(cpu);
		break;
	case 0x0B:
		DEC_ss(&cpu->BC);
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
		DJNZ_e(cpu);
		break;
	case 0x11:
		LD_dd_nn(cpu, &cpu->DE);
		break;
	case 0x12:
		LD_cDE_A(cpu);
		break;
	case 0x13:
		INC_ss(&cpu->DE);
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
		JR_e(cpu);
		break;
	case 0x19:
		ADD_HL_ss(cpu, &cpu->DE);
		break;
	case 0x1A:
		LD_A_cDE(cpu);
		break;
	case 0x1B:
		DEC_ss(&cpu->DE);
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
		JR_cc_e(cpu, !cpu->flags.Z);
		break;
	case 0x21:
		LD_dd_nn(cpu, &cpu->HL);
		break;
	case 0x22:
		LD_cnn_HL(cpu);
		break;
	case 0x23:
		INC_ss(&cpu->HL);
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
		JR_cc_e(cpu, cpu->flags.Z);
		break;
	case 0x29:
		ADD_HL_ss(cpu, &cpu->HL);
		break;
	case 0x2A:
		LD_HL_cnn(cpu);
		break;
	case 0x2B:
		DEC_ss(&cpu->HL);
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
		JR_cc_e(cpu, !cpu->flags.C);
		break;
	case 0x31:
		LD_dd_nn(cpu, &cpu->SP);
		break;
	case 0x32:
		LD_cnn_A(cpu);
		break;
	case 0x33:
		INC_ss(&cpu->SP);
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
		JR_cc_e(cpu, cpu->flags.C);
		break;
	case 0x39:
		ADD_HL_ss(cpu, &cpu->SP);
		break;
	case 0x3A:
		LD_A_cnn(cpu);
		break;
	case 0x3B:
		DEC_ss(&cpu->SP);
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
		LD_r_r(&cpu->B, &cpu->B);
		break;
	case 0x41:
		LD_r_r(&cpu->B, &cpu->C);
		break;
	case 0x42:
		LD_r_r(&cpu->B, &cpu->D);
		break;
	case 0x43:
		LD_r_r(&cpu->B, &cpu->E);
		break;
	case 0x44:
		LD_r_r(&cpu->B, &cpu->H);
		break;
	case 0x45:
		LD_r_r(&cpu->B, &cpu->L);
		break;
	case 0x46:
		LD_r_cHL(cpu, &cpu->B);
		break;
	case 0x47:
		LD_r_r(&cpu->B, &cpu->A);
		break;
	case 0x48:
		LD_r_r(&cpu->C, &cpu->B);
		break;
	case 0x49:
		LD_r_r(&cpu->C, &cpu->C);
		break;
	case 0x4A:
		LD_r_r(&cpu->C, &cpu->D);
		break;
	case 0x4B:
		LD_r_r(&cpu->C, &cpu->E);
		break;
	case 0x4C:
		LD_r_r(&cpu->C, &cpu->H);
		break;
	case 0x4D:
		LD_r_r(&cpu->C, &cpu->L);
		break;
	case 0x4E:
		LD_r_cHL(cpu, &cpu->C);
		break;
	case 0x4F:
		LD_r_r(&cpu->C, &cpu->A);
		break;
	case 0x50:
		LD_r_r(&cpu->D, &cpu->B);
		break;
	case 0x51:
		LD_r_r(&cpu->D, &cpu->C);
		break;
	case 0x52:
		LD_r_r(&cpu->D, &cpu->D);
		break;
	case 0x53:
		LD_r_r(&cpu->D, &cpu->E);
		break;
	case 0x54:
		LD_r_r(&cpu->D, &cpu->H);
		break;
	case 0x55:
		LD_r_r(&cpu->D, &cpu->L);
		break;
	case 0x56:
		LD_r_cHL(cpu, &cpu->D);
		break;
	case 0x57:
		LD_r_r(&cpu->D, &cpu->A);
		break;
	case 0x58:
		LD_r_r(&cpu->E, &cpu->B);
		break;
	case 0x59:
		LD_r_r(&cpu->E, &cpu->C);
		break;
	case 0x5A:
		LD_r_r(&cpu->E, &cpu->D);
		break;
	case 0x5B:
		LD_r_r(&cpu->E, &cpu->E);
		break;
	case 0x5C:
		LD_r_r(&cpu->E, &cpu->H);
		break;
	case 0x5D:
		LD_r_r(&cpu->E, &cpu->L);
		break;
	case 0x5E:
		LD_r_cHL(cpu, &cpu->E);
		break;
	case 0x5F:
		LD_r_r(&cpu->E, &cpu->A);
		break;
	case 0x60:
		LD_r_r(&cpu->H, &cpu->B);
		break;
	case 0x61:
		LD_r_r(&cpu->H, &cpu->C);
		break;
	case 0x62:
		LD_r_r(&cpu->H, &cpu->D);
		break;
	case 0x63:
		LD_r_r(&cpu->H, &cpu->E);
		break;
	case 0x64:
		LD_r_r(&cpu->H, &cpu->H);
		break;
	case 0x65:
		LD_r_r(&cpu->H, &cpu->L);
		break;
	case 0x66:
		LD_r_cHL(cpu, &cpu->H);
		break;
	case 0x67:
		LD_r_r(&cpu->H, &cpu->A);
		break;
	case 0x68:
		LD_r_r(&cpu->L, &cpu->B);
		break;
	case 0x69:
		LD_r_r(&cpu->L, &cpu->C);
		break;
	case 0x6A:
		LD_r_r(&cpu->L, &cpu->D);
		break;
	case 0x6B:
		LD_r_r(&cpu->L, &cpu->E);
		break;
	case 0x6C:
		LD_r_r(&cpu->L, &cpu->H);
		break;
	case 0x6D:
		LD_r_r(&cpu->L, &cpu->L);
		break;
	case 0x6E:
		LD_r_cHL(cpu, &cpu->L);
		break;
	case 0x6F:
		LD_r_r(&cpu->L, &cpu->A);
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
		LD_r_r(&cpu->A, &cpu->B);
		break;
	case 0x79:
		LD_r_r(&cpu->A, &cpu->C);
		break;
	case 0x7A:
		LD_r_r(&cpu->A, &cpu->D);
		break;
	case 0x7B:
		LD_r_r(&cpu->A, &cpu->E);
		break;
	case 0x7C:
		LD_r_r(&cpu->A, &cpu->H);
		break;
	case 0x7D:
		LD_r_r(&cpu->A, &cpu->L);
		break;
	case 0x7E:
		LD_r_cHL(cpu, &cpu->A);
		break;
	case 0x7F:
		LD_r_r(&cpu->A, &cpu->A);
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
		RET_cc(cpu, !cpu->flags.Z);
		break;
	case 0xC1:
		POP_qq(cpu, &cpu->BC);
		break;
	case 0xC2:
		JP_cc_nn(cpu, !cpu->flags.Z);
		break;
	case 0xC3:
		JP_nn(cpu);
		break;
	case 0xC4:
		CALL_cc_nn(cpu, !cpu->flags.Z);
		break;
	case 0xC5:
		PUSH_qq(cpu, &cpu->BC);
		break;
	case 0xC6:
		ADD_A_n(cpu);
		break;
	case 0xC7:
		RST_p(cpu, 0x00);
		break;
	case 0xC8:
		RET_cc(cpu, cpu->flags.Z);
		break;
	case 0xC9:
		RET(cpu);
		break;
	case 0xCA:
		JP_cc_nn(cpu, cpu->flags.Z);
		break;
	case 0xCB:
		z80_opcode_CB(cpu);
		break;
	case 0xCC:
		CALL_cc_nn(cpu, cpu->flags.Z);
		break;
	case 0xCD:
		CALL_nn(cpu);
		break;
	case 0xCE:
		ADC_A_n(cpu);
		break;
	case 0xCF:
		RST_p(cpu, 0x08);
		break;
	case 0xD0:
		RET_cc(cpu, !cpu->flags.C);
		break;
	case 0xD1:
		POP_qq(cpu, &cpu->DE);
		break;
	case 0xD2:
		JP_cc_nn(cpu, !cpu->flags.C);
		break;
	case 0xD3:
		OUT_cn_A(cpu);
		break;
	case 0xD4:
		CALL_cc_nn(cpu, !cpu->flags.C);
		break;
	case 0xD5:
		PUSH_qq(cpu, &cpu->DE);
		break;
	case 0xD6:
		SUB_A_n(cpu);
		break;
	case 0xD7:
		RST_p(cpu, 0x10);
		break;
	case 0xD8:
		RET_cc(cpu, cpu->flags.C);
		break;
	case 0xD9:
		EXX(cpu);
		break;
	case 0xDA:
		JP_cc_nn(cpu, cpu->flags.C);
		break;
	case 0xDB:
		IN_A_cn(cpu);
		break;
	case 0xDC:
		CALL_cc_nn(cpu, cpu->flags.C);
		break;
	case 0xDD:
		z80_opcode_DDFD(cpu, opcode);
		break;
	case 0xDE:
		SBC_A_n(cpu);
		break;
	case 0xDF:
		RST_p(cpu, 0x18);
		break;
	case 0xE0:
		RET_cc(cpu, !cpu->flags.PV);
		break;
	case 0xE1:
		POP_qq(cpu, &cpu->HL);
		break;
	case 0xE2:
		JP_cc_nn(cpu, !cpu->flags.PV);
		break;
	case 0xE3:
		EX_cSP_HL(cpu);
		break;
	case 0xE5:
		PUSH_qq(cpu, &cpu->HL);
		break;
	case 0xE6:
		AND_n(cpu);
		break;
	case 0xE7:
		RST_p(cpu, 0x20);
		break;
	case 0xE8:
		RET_cc(cpu, cpu->flags.PV);
		break;
	case 0xE9:
		JP_HL(cpu);
		break;
	case 0xEA:
		JP_cc_nn(cpu, cpu->flags.PV);
		break;
	case 0xEB:
		EX_DE_HL(cpu);
		break;
	case 0xED:
		z80_opcode_ED(cpu);
		break;
	case 0xEE:
		XOR_n(cpu);
		break;
	case 0xEF:
		RST_p(cpu, 0x28);
		break;
	case 0xF0:
		RET_cc(cpu, !cpu->flags.S);
		break;
	case 0xF1:
		POP_qq(cpu, &cpu->AF);
		break;
	case 0xF2:
		JP_cc_nn(cpu, !cpu->flags.S);
		break;
	case 0xF3:
		DI(cpu);
		break;
	case 0xF4:
		CALL_cc_nn(cpu, !cpu->flags.S);
		break;
	case 0xF5:
		PUSH_qq(cpu, &cpu->AF);
		break;
	case 0xF6:
		OR_n(cpu);
		break;
	case 0xF7:
		RST_p(cpu, 0x30);
		break;
	case 0xF8:
		RET_cc(cpu, cpu->flags.S);
		break;
	case 0xF9:
		LD_SP_HL(cpu);
		break;
	case 0xFA:
		JP_cc_nn(cpu, cpu->flags.S);
		break;
	case 0xFB:
		EI(cpu);
		break;
	case 0xFC:
		CALL_cc_nn(cpu, cpu->flags.S);
		break;
	case 0xFD:
		z80_opcode_DDFD(cpu, opcode);
		break;
	case 0xFE:
		CP_n(cpu);
		break;
	case 0xFF:
		RST_p(cpu, 0x38);
		break;
	default:
		LOG_W("z80: unknown opcode (%02x)!\n", opcode);
		clock_consume(1);
		break;
	}
}

void z80_opcode_CB(struct z80 *cpu)
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
		SL1_r(cpu, &cpu->B);
		break;
	case 0x31:
		SL1_r(cpu, &cpu->C);
		break;
	case 0x32:
		SL1_r(cpu, &cpu->D);
		break;
	case 0x33:
		SL1_r(cpu, &cpu->E);
		break;
	case 0x34:
		SL1_r(cpu, &cpu->H);
		break;
	case 0x35:
		SL1_r(cpu, &cpu->L);
		break;
	case 0x36:
		SL1_cHL(cpu);
		break;
	case 0x37:
		SL1_r(cpu, &cpu->A);
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
		BIT_b_r(cpu, 0, &cpu->B);
		break;
	case 0x41:
		BIT_b_r(cpu, 0, &cpu->C);
		break;
	case 0x42:
		BIT_b_r(cpu, 0, &cpu->D);
		break;
	case 0x43:
		BIT_b_r(cpu, 0, &cpu->E);
		break;
	case 0x44:
		BIT_b_r(cpu, 0, &cpu->H);
		break;
	case 0x45:
		BIT_b_r(cpu, 0, &cpu->L);
		break;
	case 0x46:
		BIT_b_cHL(cpu, 0);
		break;
	case 0x47:
		BIT_b_r(cpu, 0, &cpu->A);
		break;
	case 0x48:
		BIT_b_r(cpu, 1, &cpu->B);
		break;
	case 0x49:
		BIT_b_r(cpu, 1, &cpu->C);
		break;
	case 0x4A:
		BIT_b_r(cpu, 1, &cpu->D);
		break;
	case 0x4B:
		BIT_b_r(cpu, 1, &cpu->E);
		break;
	case 0x4C:
		BIT_b_r(cpu, 1, &cpu->H);
		break;
	case 0x4D:
		BIT_b_r(cpu, 1, &cpu->L);
		break;
	case 0x4E:
		BIT_b_cHL(cpu, 1);
		break;
	case 0x4F:
		BIT_b_r(cpu, 1, &cpu->A);
		break;
	case 0x50:
		BIT_b_r(cpu, 2, &cpu->B);
		break;
	case 0x51:
		BIT_b_r(cpu, 2, &cpu->C);
		break;
	case 0x52:
		BIT_b_r(cpu, 2, &cpu->D);
		break;
	case 0x53:
		BIT_b_r(cpu, 2, &cpu->E);
		break;
	case 0x54:
		BIT_b_r(cpu, 2, &cpu->H);
		break;
	case 0x55:
		BIT_b_r(cpu, 2, &cpu->L);
		break;
	case 0x56:
		BIT_b_cHL(cpu, 2);
		break;
	case 0x57:
		BIT_b_r(cpu, 2, &cpu->A);
		break;
	case 0x58:
		BIT_b_r(cpu, 3, &cpu->B);
		break;
	case 0x59:
		BIT_b_r(cpu, 3, &cpu->C);
		break;
	case 0x5A:
		BIT_b_r(cpu, 3, &cpu->D);
		break;
	case 0x5B:
		BIT_b_r(cpu, 3, &cpu->E);
		break;
	case 0x5C:
		BIT_b_r(cpu, 3, &cpu->H);
		break;
	case 0x5D:
		BIT_b_r(cpu, 3, &cpu->L);
		break;
	case 0x5E:
		BIT_b_cHL(cpu, 3);
		break;
	case 0x5F:
		BIT_b_r(cpu, 3, &cpu->A);
		break;
	case 0x60:
		BIT_b_r(cpu, 4, &cpu->B);
		break;
	case 0x61:
		BIT_b_r(cpu, 4, &cpu->C);
		break;
	case 0x62:
		BIT_b_r(cpu, 4, &cpu->D);
		break;
	case 0x63:
		BIT_b_r(cpu, 4, &cpu->E);
		break;
	case 0x64:
		BIT_b_r(cpu, 4, &cpu->H);
		break;
	case 0x65:
		BIT_b_r(cpu, 4, &cpu->L);
		break;
	case 0x66:
		BIT_b_cHL(cpu, 4);
		break;
	case 0x67:
		BIT_b_r(cpu, 4, &cpu->A);
		break;
	case 0x68:
		BIT_b_r(cpu, 5, &cpu->B);
		break;
	case 0x69:
		BIT_b_r(cpu, 5, &cpu->C);
		break;
	case 0x6A:
		BIT_b_r(cpu, 5, &cpu->D);
		break;
	case 0x6B:
		BIT_b_r(cpu, 5, &cpu->E);
		break;
	case 0x6C:
		BIT_b_r(cpu, 5, &cpu->H);
		break;
	case 0x6D:
		BIT_b_r(cpu, 5, &cpu->L);
		break;
	case 0x6E:
		BIT_b_cHL(cpu, 5);
		break;
	case 0x6F:
		BIT_b_r(cpu, 5, &cpu->A);
		break;
	case 0x70:
		BIT_b_r(cpu, 6, &cpu->B);
		break;
	case 0x71:
		BIT_b_r(cpu, 6, &cpu->C);
		break;
	case 0x72:
		BIT_b_r(cpu, 6, &cpu->D);
		break;
	case 0x73:
		BIT_b_r(cpu, 6, &cpu->E);
		break;
	case 0x74:
		BIT_b_r(cpu, 6, &cpu->H);
		break;
	case 0x75:
		BIT_b_r(cpu, 6, &cpu->L);
		break;
	case 0x76:
		BIT_b_cHL(cpu, 6);
		break;
	case 0x77:
		BIT_b_r(cpu, 6, &cpu->A);
		break;
	case 0x78:
		BIT_b_r(cpu, 7, &cpu->B);
		break;
	case 0x79:
		BIT_b_r(cpu, 7, &cpu->C);
		break;
	case 0x7A:
		BIT_b_r(cpu, 7, &cpu->D);
		break;
	case 0x7B:
		BIT_b_r(cpu, 7, &cpu->E);
		break;
	case 0x7C:
		BIT_b_r(cpu, 7, &cpu->H);
		break;
	case 0x7D:
		BIT_b_r(cpu, 7, &cpu->L);
		break;
	case 0x7E:
		BIT_b_cHL(cpu, 7);
		break;
	case 0x7F:
		BIT_b_r(cpu, 7, &cpu->A);
		break;
	case 0x80:
		RES_b_r(0, &cpu->B);
		break;
	case 0x81:
		RES_b_r(0, &cpu->C);
		break;
	case 0x82:
		RES_b_r(0, &cpu->D);
		break;
	case 0x83:
		RES_b_r(0, &cpu->E);
		break;
	case 0x84:
		RES_b_r(0, &cpu->H);
		break;
	case 0x85:
		RES_b_r(0, &cpu->L);
		break;
	case 0x86:
		RES_b_cHL(cpu, 0);
		break;
	case 0x87:
		RES_b_r(0, &cpu->A);
		break;
	case 0x88:
		RES_b_r(1, &cpu->B);
		break;
	case 0x89:
		RES_b_r(1, &cpu->C);
		break;
	case 0x8A:
		RES_b_r(1, &cpu->D);
		break;
	case 0x8B:
		RES_b_r(1, &cpu->E);
		break;
	case 0x8C:
		RES_b_r(1, &cpu->H);
		break;
	case 0x8D:
		RES_b_r(1, &cpu->L);
		break;
	case 0x8E:
		RES_b_cHL(cpu, 1);
		break;
	case 0x8F:
		RES_b_r(1, &cpu->A);
		break;
	case 0x90:
		RES_b_r(2, &cpu->B);
		break;
	case 0x91:
		RES_b_r(2, &cpu->C);
		break;
	case 0x92:
		RES_b_r(2, &cpu->D);
		break;
	case 0x93:
		RES_b_r(2, &cpu->E);
		break;
	case 0x94:
		RES_b_r(2, &cpu->H);
		break;
	case 0x95:
		RES_b_r(2, &cpu->L);
		break;
	case 0x96:
		RES_b_cHL(cpu, 2);
		break;
	case 0x97:
		RES_b_r(2, &cpu->A);
		break;
	case 0x98:
		RES_b_r(3, &cpu->B);
		break;
	case 0x99:
		RES_b_r(3, &cpu->C);
		break;
	case 0x9A:
		RES_b_r(3, &cpu->D);
		break;
	case 0x9B:
		RES_b_r(3, &cpu->E);
		break;
	case 0x9C:
		RES_b_r(3, &cpu->H);
		break;
	case 0x9D:
		RES_b_r(3, &cpu->L);
		break;
	case 0x9E:
		RES_b_cHL(cpu, 3);
		break;
	case 0x9F:
		RES_b_r(3, &cpu->A);
		break;
	case 0xA0:
		RES_b_r(4, &cpu->B);
		break;
	case 0xA1:
		RES_b_r(4, &cpu->C);
		break;
	case 0xA2:
		RES_b_r(4, &cpu->D);
		break;
	case 0xA3:
		RES_b_r(4, &cpu->E);
		break;
	case 0xA4:
		RES_b_r(4, &cpu->H);
		break;
	case 0xA5:
		RES_b_r(4, &cpu->L);
		break;
	case 0xA6:
		RES_b_cHL(cpu, 4);
		break;
	case 0xA7:
		RES_b_r(4, &cpu->A);
		break;
	case 0xA8:
		RES_b_r(5, &cpu->B);
		break;
	case 0xA9:
		RES_b_r(5, &cpu->C);
		break;
	case 0xAA:
		RES_b_r(5, &cpu->D);
		break;
	case 0xAB:
		RES_b_r(5, &cpu->E);
		break;
	case 0xAC:
		RES_b_r(5, &cpu->H);
		break;
	case 0xAD:
		RES_b_r(5, &cpu->L);
		break;
	case 0xAE:
		RES_b_cHL(cpu, 5);
		break;
	case 0xAF:
		RES_b_r(5, &cpu->A);
		break;
	case 0xB0:
		RES_b_r(6, &cpu->B);
		break;
	case 0xB1:
		RES_b_r(6, &cpu->C);
		break;
	case 0xB2:
		RES_b_r(6, &cpu->D);
		break;
	case 0xB3:
		RES_b_r(6, &cpu->E);
		break;
	case 0xB4:
		RES_b_r(6, &cpu->H);
		break;
	case 0xB5:
		RES_b_r(6, &cpu->L);
		break;
	case 0xB6:
		RES_b_cHL(cpu, 6);
		break;
	case 0xB7:
		RES_b_r(6, &cpu->A);
		break;
	case 0xB8:
		RES_b_r(7, &cpu->B);
		break;
	case 0xB9:
		RES_b_r(7, &cpu->C);
		break;
	case 0xBA:
		RES_b_r(7, &cpu->D);
		break;
	case 0xBB:
		RES_b_r(7, &cpu->E);
		break;
	case 0xBC:
		RES_b_r(7, &cpu->H);
		break;
	case 0xBD:
		RES_b_r(7, &cpu->L);
		break;
	case 0xBE:
		RES_b_cHL(cpu, 7);
		break;
	case 0xBF:
		RES_b_r(7, &cpu->A);
		break;
	case 0xC0:
		SET_b_r(0, &cpu->B);
		break;
	case 0xC1:
		SET_b_r(0, &cpu->C);
		break;
	case 0xC2:
		SET_b_r(0, &cpu->D);
		break;
	case 0xC3:
		SET_b_r(0, &cpu->E);
		break;
	case 0xC4:
		SET_b_r(0, &cpu->H);
		break;
	case 0xC5:
		SET_b_r(0, &cpu->L);
		break;
	case 0xC6:
		SET_b_cHL(cpu, 0);
		break;
	case 0xC7:
		SET_b_r(0, &cpu->A);
		break;
	case 0xC8:
		SET_b_r(1, &cpu->B);
		break;
	case 0xC9:
		SET_b_r(1, &cpu->C);
		break;
	case 0xCA:
		SET_b_r(1, &cpu->D);
		break;
	case 0xCB:
		SET_b_r(1, &cpu->E);
		break;
	case 0xCC:
		SET_b_r(1, &cpu->H);
		break;
	case 0xCD:
		SET_b_r(1, &cpu->L);
		break;
	case 0xCE:
		SET_b_cHL(cpu, 1);
		break;
	case 0xCF:
		SET_b_r(1, &cpu->A);
		break;
	case 0xD0:
		SET_b_r(2, &cpu->B);
		break;
	case 0xD1:
		SET_b_r(2, &cpu->C);
		break;
	case 0xD2:
		SET_b_r(2, &cpu->D);
		break;
	case 0xD3:
		SET_b_r(2, &cpu->E);
		break;
	case 0xD4:
		SET_b_r(2, &cpu->H);
		break;
	case 0xD5:
		SET_b_r(2, &cpu->L);
		break;
	case 0xD6:
		SET_b_cHL(cpu, 2);
		break;
	case 0xD7:
		SET_b_r(2, &cpu->A);
		break;
	case 0xD8:
		SET_b_r(3, &cpu->B);
		break;
	case 0xD9:
		SET_b_r(3, &cpu->C);
		break;
	case 0xDA:
		SET_b_r(3, &cpu->D);
		break;
	case 0xDB:
		SET_b_r(3, &cpu->E);
		break;
	case 0xDC:
		SET_b_r(3, &cpu->H);
		break;
	case 0xDD:
		SET_b_r(3, &cpu->L);
		break;
	case 0xDE:
		SET_b_cHL(cpu, 3);
		break;
	case 0xDF:
		SET_b_r(3, &cpu->A);
		break;
	case 0xE0:
		SET_b_r(4, &cpu->B);
		break;
	case 0xE1:
		SET_b_r(4, &cpu->C);
		break;
	case 0xE2:
		SET_b_r(4, &cpu->D);
		break;
	case 0xE3:
		SET_b_r(4, &cpu->E);
		break;
	case 0xE4:
		SET_b_r(4, &cpu->H);
		break;
	case 0xE5:
		SET_b_r(4, &cpu->L);
		break;
	case 0xE6:
		SET_b_cHL(cpu, 4);
		break;
	case 0xE7:
		SET_b_r(4, &cpu->A);
		break;
	case 0xE8:
		SET_b_r(5, &cpu->B);
		break;
	case 0xE9:
		SET_b_r(5, &cpu->C);
		break;
	case 0xEA:
		SET_b_r(5, &cpu->D);
		break;
	case 0xEB:
		SET_b_r(5, &cpu->E);
		break;
	case 0xEC:
		SET_b_r(5, &cpu->H);
		break;
	case 0xED:
		SET_b_r(5, &cpu->L);
		break;
	case 0xEE:
		SET_b_cHL(cpu, 5);
		break;
	case 0xEF:
		SET_b_r(5, &cpu->A);
		break;
	case 0xF0:
		SET_b_r(6, &cpu->B);
		break;
	case 0xF1:
		SET_b_r(6, &cpu->C);
		break;
	case 0xF2:
		SET_b_r(6, &cpu->D);
		break;
	case 0xF3:
		SET_b_r(6, &cpu->E);
		break;
	case 0xF4:
		SET_b_r(6, &cpu->H);
		break;
	case 0xF5:
		SET_b_r(6, &cpu->L);
		break;
	case 0xF6:
		SET_b_cHL(cpu, 6);
		break;
	case 0xF7:
		SET_b_r(6, &cpu->A);
		break;
	case 0xF8:
		SET_b_r(7, &cpu->B);
		break;
	case 0xF9:
		SET_b_r(7, &cpu->C);
		break;
	case 0xFA:
		SET_b_r(7, &cpu->D);
		break;
	case 0xFB:
		SET_b_r(7, &cpu->E);
		break;
	case 0xFC:
		SET_b_r(7, &cpu->H);
		break;
	case 0xFD:
		SET_b_r(7, &cpu->L);
		break;
	case 0xFE:
		SET_b_cHL(cpu, 7);
		break;
	case 0xFF:
		SET_b_r(7, &cpu->A);
		break;
	default:
		LOG_W("z80: unknown CB opcode (%02x)!\n", opcode);
		clock_consume(1);
		break;
	}
}

void z80_opcode_DDFD(struct z80 *cpu, uint8_t prefix)
{
	uint16_t *reg = (prefix == 0xDD) ? &cpu->IX : &cpu->IY;
	uint8_t *low = (prefix == 0xDD) ? &cpu->lIX : &cpu->lIY;
	uint8_t *high = (prefix == 0xDD) ? &cpu->hIX : &cpu->hIY;
	uint8_t opcode;

	/* Fetch DD/FD opcode */
	opcode = memory_readb(cpu->bus_id, cpu->PC++);

	/* Execute DD/FD opcode */
	switch (opcode) {
	case 0x09:
		ADD_IXY_pp(cpu, &cpu->BC, reg);
		break;
	case 0x19:
		ADD_IXY_pp(cpu, &cpu->DE, reg);
		break;
	case 0x21:
		LD_IXY_nn(cpu, reg);
		break;
	case 0x22:
		LD_cnn_IXY(cpu, reg);
		break;
	case 0x23:
		INC_ss(reg);
		break;
	case 0x24:
		INC_r(cpu, high);
		break;
	case 0x25:
		DEC_r(cpu, high);
		break;
	case 0x26:
		LD_r_n(cpu, high);
		break;
	case 0x29:
		ADD_IXY_pp(cpu, reg, reg);
		break;
	case 0x2A:
		LD_IXY_cnn(cpu, reg);
		break;
	case 0x2B:
		DEC_IXY(reg);
		break;
	case 0x2C:
		INC_r(cpu, low);
		break;
	case 0x2D:
		DEC_r(cpu, low);
		break;
	case 0x2E:
		LD_r_n(cpu, low);
		break;
	case 0x34:
		INC_cIXYpd(cpu, reg);
		break;
	case 0x35:
		DEC_cIXYpd(cpu, reg);
		break;
	case 0x36:
		LD_cIXYpd_n(cpu, reg);
		break;
	case 0x39:
		ADD_IXY_pp(cpu, &cpu->SP, reg);
		break;
	case 0x46:
		LD_r_cIXYpd(cpu, &cpu->B, reg);
		break;
	case 0x4E:
		LD_r_cIXYpd(cpu, &cpu->C, reg);
		break;
	case 0x56:
		LD_r_cIXYpd(cpu, &cpu->D, reg);
		break;
	case 0x5E:
		LD_r_cIXYpd(cpu, &cpu->E, reg);
		break;
	case 0x66:
		LD_r_cIXYpd(cpu, &cpu->H, reg);
		break;
	case 0x67:
		LD_r_r(high, &cpu->A);
		break;
	case 0x6E:
		LD_r_cIXYpd(cpu, &cpu->L, reg);
		break;
	case 0x6F:
		LD_r_r(low, &cpu->A);
		break;
	case 0x70:
		LD_cIXYpd_r(cpu, &cpu->B, reg);
		break;
	case 0x71:
		LD_cIXYpd_r(cpu, &cpu->C, reg);
		break;
	case 0x72:
		LD_cIXYpd_r(cpu, &cpu->D, reg);
		break;
	case 0x73:
		LD_cIXYpd_r(cpu, &cpu->E, reg);
		break;
	case 0x74:
		LD_cIXYpd_r(cpu, &cpu->H, reg);
		break;
	case 0x75:
		LD_cIXYpd_r(cpu, &cpu->L, reg);
		break;
	case 0x77:
		LD_cIXYpd_r(cpu, &cpu->A, reg);
		break;
	case 0x7C:
		LD_r_r(&cpu->A, high);
		break;
	case 0x7D:
		LD_r_r(&cpu->A, low);
		break;
	case 0x7E:
		LD_r_cIXYpd(cpu, &cpu->A, reg);
		break;
	case 0x86:
		ADD_A_cIXYpd(cpu, reg);
		break;
	case 0x8E:
		ADC_A_cIXYpd(cpu, reg);
		break;
	case 0x96:
		SUB_A_cIXYpd(cpu, reg);
		break;
	case 0x9E:
		SBC_A_cIXYpd(cpu, reg);
		break;
	case 0xA6:
		AND_cIXYpd(cpu, reg);
		break;
	case 0xAE:
		XOR_cIXYpd(cpu, reg);
		break;
	case 0xB6:
		OR_cIXYpd(cpu, reg);
		break;
	case 0xBE:
		CP_IXYpd(cpu, reg);
		break;
	case 0xCB:
		z80_opcode_DDFD_CB(cpu, reg);
		break;
	case 0xE1:
		POP_IXY(cpu, reg);
		break;
	case 0xE3:
		EX_cSP_IXY(cpu, reg);
		break;
	case 0xE5:
		PUSH_IXY(cpu, reg);
		break;
	case 0xE9:
		JP_IXY(cpu, reg);
		break;
	case 0xF9:
		LD_SP_IXY(cpu, reg);
		break;
	case 0xFD:
		POP_IXY(cpu, reg);
		break;
	default:
		LOG_W("z80: unknown DD/FD opcode (%02x)!\n", opcode);
		clock_consume(1);
		break;
	}
}

void z80_opcode_DDFD_CB(struct z80 *cpu, uint16_t *reg)
{
	uint8_t opcode;

	/* Fetch DD/FD CB opcode */
	opcode = memory_readb(cpu->bus_id, cpu->PC + 1);

	/* Execute DD/FD CB opcode */
	switch (opcode) {
	case 0x06:
		RLC_cIXYpd(cpu, reg);
		break;
	case 0x0E:
		RRC_cIXYpd(cpu, reg);
		break;
	case 0x16:
		RL_cIXYpd(cpu, reg);
		break;
	case 0x1E:
		RR_cIXYpd(cpu, reg);
		break;
	case 0x26:
		SLA_cIXYpd(cpu, reg);
		break;
	case 0x2E:
		SRA_cIXYpd(cpu, reg);
		break;
	case 0x36:
		SL1_cIXYpd(cpu, reg);
		break;
	case 0x3E:
		SRL_cIXYpd(cpu, reg);
		break;
	case 0x46:
		BIT_b_cIXYpd(cpu, 0, reg);
		break;
	case 0x4E:
		BIT_b_cIXYpd(cpu, 1, reg);
		break;
	case 0x56:
		BIT_b_cIXYpd(cpu, 2, reg);
		break;
	case 0x5E:
		BIT_b_cIXYpd(cpu, 3, reg);
		break;
	case 0x66:
		BIT_b_cIXYpd(cpu, 4, reg);
		break;
	case 0x6E:
		BIT_b_cIXYpd(cpu, 5, reg);
		break;
	case 0x76:
		BIT_b_cIXYpd(cpu, 6, reg);
		break;
	case 0x7E:
		BIT_b_cIXYpd(cpu, 7, reg);
		break;
	case 0x86:
		RES_b_cIXYpd(cpu, 0, reg);
		break;
	case 0x8E:
		RES_b_cIXYpd(cpu, 1, reg);
		break;
	case 0x96:
		RES_b_cIXYpd(cpu, 2, reg);
		break;
	case 0x9E:
		RES_b_cIXYpd(cpu, 3, reg);
		break;
	case 0xA6:
		RES_b_cIXYpd(cpu, 4, reg);
		break;
	case 0xAE:
		RES_b_cIXYpd(cpu, 5, reg);
		break;
	case 0xB6:
		RES_b_cIXYpd(cpu, 6, reg);
		break;
	case 0xBE:
		RES_b_cIXYpd(cpu, 7, reg);
		break;
	case 0xC6:
		SET_b_cIXYpd(cpu, 0, reg);
		break;
	case 0xCE:
		SET_b_cIXYpd(cpu, 1, reg);
		break;
	case 0xD6:
		SET_b_cIXYpd(cpu, 2, reg);
		break;
	case 0xDE:
		SET_b_cIXYpd(cpu, 3, reg);
		break;
	case 0xE6:
		SET_b_cIXYpd(cpu, 4, reg);
		break;
	case 0xEE:
		SET_b_cIXYpd(cpu, 5, reg);
		break;
	case 0xF6:
		SET_b_cIXYpd(cpu, 6, reg);
		break;
	case 0xFE:
		SET_b_cIXYpd(cpu, 7, reg);
		break;
	default:
		LOG_W("z80: unknown DD/FD CB opcode (%02x)!\n", opcode);
		clock_consume(1);
		break;
	}

	/* Increment PC */
	cpu->PC++;
}

void z80_opcode_ED(struct z80 *cpu)
{
	uint8_t opcode;

	/* Fetch ED opcode */
	opcode = memory_readb(cpu->bus_id, cpu->PC++);

	/* Execute ED opcode */
	switch (opcode) {
	case 0x40:
		IN_r_cC(cpu, &cpu->B);
		break;
	case 0x41:
		OUT_cC_r(cpu, &cpu->B);
		break;
	case 0x42:
		SBC_HL_rr(cpu, &cpu->BC);
		break;
	case 0x43:
		LD_cnn_dd(cpu, &cpu->BC);
		break;
	case 0x44:
		NEG(cpu);
		break;
	case 0x45:
		RETN(cpu);
		break;
	case 0x46:
		IM_0(cpu);
		break;
	case 0x47:
		LD_I_A(cpu);
		break;
	case 0x48:
		IN_r_cC(cpu, &cpu->C);
		break;
	case 0x49:
		OUT_cC_r(cpu, &cpu->C);
		break;
	case 0x4A:
		ADC_HL_rr(cpu, &cpu->BC);
		break;
	case 0x4B:
		LD_dd_cnn(cpu, &cpu->BC);
		break;
	case 0x4D:
		RETI(cpu);
		break;
	case 0x4F:
		LD_R_A(cpu);
		break;
	case 0x50:
		IN_r_cC(cpu, &cpu->D);
		break;
	case 0x51:
		OUT_cC_r(cpu, &cpu->D);
		break;
	case 0x52:
		SBC_HL_rr(cpu, &cpu->DE);
		break;
	case 0x53:
		LD_cnn_dd(cpu, &cpu->DE);
		break;
	case 0x56:
		IM_1(cpu);
		break;
	case 0x57:
		LD_A_I(cpu);
		break;
	case 0x58:
		IN_r_cC(cpu, &cpu->E);
		break;
	case 0x59:
		OUT_cC_r(cpu, &cpu->E);
		break;
	case 0x5A:
		ADC_HL_rr(cpu, &cpu->DE);
		break;
	case 0x5B:
		LD_dd_cnn(cpu, &cpu->DE);
		break;
	case 0x5E:
		IM_2(cpu);
		break;
	case 0x5F:
		LD_A_R(cpu);
		break;
	case 0x60:
		IN_r_cC(cpu, &cpu->H);
		break;
	case 0x61:
		OUT_cC_r(cpu, &cpu->H);
		break;
	case 0x67:
		RRD(cpu);
		break;
	case 0x68:
		IN_r_cC(cpu, &cpu->L);
		break;
	case 0x69:
		OUT_cC_r(cpu, &cpu->L);
		break;
	case 0x6A:
		ADC_HL_rr(cpu, &cpu->HL);
		break;
	case 0x6F:
		RLD(cpu);
		break;
	case 0x73:
		LD_cnn_dd(cpu, &cpu->SP);
		break;
	case 0x78:
		IN_r_cC(cpu, &cpu->A);
		break;
	case 0x79:
		OUT_cC_r(cpu, &cpu->A);
		break;
	case 0x7B:
		LD_dd_cnn(cpu, &cpu->SP);
		break;
	case 0xA0:
		LDI(cpu);
		break;
	case 0xA1:
		CPI(cpu);
		break;
	case 0xA2:
		INI(cpu);
		break;
	case 0xA3:
		OUTI(cpu);
		break;
	case 0xA8:
		LDD(cpu);
		break;
	case 0xA9:
		CPD(cpu);
		break;
	case 0xAB:
		OUTD(cpu);
		break;
	case 0xAA:
		IND(cpu);
		break;
	case 0xB0:
		LDIR(cpu);
		break;
	case 0xB1:
		CPIR(cpu);
		break;
	case 0xB2:
		INIR(cpu);
		break;
	case 0xB3:
		OTIR(cpu);
		break;
	case 0xB8:
		LDDR(cpu);
		break;
	case 0xB9:
		CPDR(cpu);
		break;
	case 0xBA:
		INDR(cpu);
		break;
	case 0xBB:
		OTDR(cpu);
		break;
	default:
		LOG_W("z80: unknown ED opcode (%02x)!\n", opcode);
		clock_consume(1);
		break;
	}
}

bool z80_init(struct cpu_instance *instance)
{
	struct z80 *cpu;
	struct resource *res;

	/* Allocate z80 structure and set private data */
	cpu = calloc(1, sizeof(struct z80));
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
	cpu->clock.tick = (clock_tick_t)z80_tick;
	clock_add(&cpu->clock);

	return true;
}

void z80_reset(struct cpu_instance *instance)
{
	struct z80 *cpu = instance->priv_data;

	/* Initialize processor data */
	cpu->AF = 0;
	cpu->BC = 0;
	cpu->DE = 0;
	cpu->HL = 0;
	cpu->A2F2 = 0;
	cpu->B2C2 = 0;
	cpu->D2E2 = 0;
	cpu->H2L2 = 0;
	cpu->PC = 0;
	cpu->SP = 0xDFF0;
	cpu->IX = 0xFFFF;
	cpu->IY = 0xFFFF;
	cpu->IFF1 = false;
	cpu->IFF2 = false;
	cpu->irq_delay = false;
	cpu->interrupt_mode = 0;
	cpu->irq_pending = false;
	cpu->nmi_pending = false;
	cpu->halted = false;

	/* Enable clock */
	cpu->clock.enabled = true;
}

void z80_interrupt(struct cpu_instance *instance, int irq)
{
	struct z80 *cpu = instance->priv_data;

	/* Handle IRQ or NMI */
	if (irq == IRQ_N)
		cpu->irq_pending = true;
	else if (irq == NMI_N)
		cpu->nmi_pending = true;
}

void z80_deinit(struct cpu_instance *instance)
{
	struct z80 *cpu = instance->priv_data;
	free(cpu);
}

CPU_START(z80)
	.init = z80_init,
	.reset = z80_reset,
	.interrupt = z80_interrupt,
	.deinit = z80_deinit
CPU_END

