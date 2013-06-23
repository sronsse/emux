#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clock.h>
#include <cpu.h>
#include <memory.h>

#define NMI_VECTOR		0xFFFA
#define RESET_VECTOR		0xFFFC
#define INTERRUPT_VECTOR	0xFFFE
#define STACK_START		0x100
#define ZP_SIZE			0x100

struct rp2a03 {
	uint8_t A;
	uint8_t X;
	uint8_t Y;
	uint16_t PC;
	uint8_t S;
	union {
		uint8_t P;
		struct {
			uint8_t C:1;
			uint8_t Z:1;
			uint8_t I:1;
			uint8_t D:1;
			uint8_t B:1;
			uint8_t unused:1;
			uint8_t V:1;
			uint8_t N:1;
		};
	};
	int bus_id;
	uint8_t n_cycles;
	struct clock clock;
};

static bool rp2a03_init(struct cpu_instance *instance);
static void rp2a03_deinit(struct cpu_instance *instance);
static void rp2a03_tick(clock_data_t *data);
static inline void ADC_A(struct rp2a03 *rp2a03);
static inline void ADC_AX(struct rp2a03 *rp2a03);
static inline void ADC_AY(struct rp2a03 *rp2a03);
static inline void ADC_I(struct rp2a03 *rp2a03);
static inline void ADC_IX(struct rp2a03 *rp2a03);
static inline void ADC_IY(struct rp2a03 *rp2a03);
static inline void ADC_ZP(struct rp2a03 *rp2a03);
static inline void ADC_ZPX(struct rp2a03 *rp2a03);
static inline void AND(struct rp2a03 *rp2a03, uint8_t b);
static inline void AND_A(struct rp2a03 *rp2a03);
static inline void AND_AX(struct rp2a03 *rp2a03);
static inline void AND_AY(struct rp2a03 *rp2a03);
static inline void AND_I(struct rp2a03 *rp2a03);
static inline void AND_IX(struct rp2a03 *rp2a03);
static inline void AND_IY(struct rp2a03 *rp2a03);
static inline void AND_ZP(struct rp2a03 *rp2a03);
static inline void AND_ZPX(struct rp2a03 *rp2a03);
static inline void ASL_ACC(struct rp2a03 *rp2a03);
static inline void ASL_A(struct rp2a03 *rp2a03);
static inline void ASL_AX(struct rp2a03 *rp2a03);
static inline void ASL_ZP(struct rp2a03 *rp2a03);
static inline void ASL_ZPX(struct rp2a03 *rp2a03);
static inline void BCC(struct rp2a03 *rp2a03);
static inline void BCS(struct rp2a03 *rp2a03);
static inline void BEQ(struct rp2a03 *rp2a03);
static inline void BIT(struct rp2a03 *rp2a03, uint8_t b);
static inline void BIT_A(struct rp2a03 *rp2a03);
static inline void BIT_ZP(struct rp2a03 *rp2a03);
static inline void BMI(struct rp2a03 *rp2a03);
static inline void BNE(struct rp2a03 *rp2a03);
static inline void BPL(struct rp2a03 *rp2a03);
static inline void BRK(struct rp2a03 *rp2a03);
static inline void BVC(struct rp2a03 *rp2a03);
static inline void BVS(struct rp2a03 *rp2a03);
static inline void CLC(struct rp2a03 *rp2a03);
static inline void CLD(struct rp2a03 *rp2a03);
static inline void CLV(struct rp2a03 *rp2a03);
static inline void CMP(struct rp2a03 *rp2a03, uint8_t b);
static inline void CMP_A(struct rp2a03 *rp2a03);
static inline void CMP_AX(struct rp2a03 *rp2a03);
static inline void CMP_AY(struct rp2a03 *rp2a03);
static inline void CMP_I(struct rp2a03 *rp2a03);
static inline void CMP_IX(struct rp2a03 *rp2a03);
static inline void CMP_IY(struct rp2a03 *rp2a03);
static inline void CMP_ZP(struct rp2a03 *rp2a03);
static inline void CMP_ZPX(struct rp2a03 *rp2a03);
static inline void CPX(struct rp2a03 *rp2a03, uint8_t b);
static inline void CPX_A(struct rp2a03 *rp2a03);
static inline void CPX_I(struct rp2a03 *rp2a03);
static inline void CPX_ZP(struct rp2a03 *rp2a03);
static inline void CPY(struct rp2a03 *rp2a03, uint8_t b);
static inline void CPY_A(struct rp2a03 *rp2a03);
static inline void CPY_I(struct rp2a03 *rp2a03);
static inline void CPY_ZP(struct rp2a03 *rp2a03);
static inline void DEC(struct rp2a03 *rp2a03, uint16_t address);
static inline void DEC_A(struct rp2a03 *rp2a03);
static inline void DEC_AX(struct rp2a03 *rp2a03);
static inline void DEC_ZP(struct rp2a03 *rp2a03);
static inline void DEC_ZPX(struct rp2a03 *rp2a03);
static inline void DEX(struct rp2a03 *rp2a03);
static inline void DEY(struct rp2a03 *rp2a03);
static inline void EOR(struct rp2a03 *rp2a03, uint8_t b);
static inline void EOR_A(struct rp2a03 *rp2a03);
static inline void EOR_AX(struct rp2a03 *rp2a03);
static inline void EOR_AY(struct rp2a03 *rp2a03);
static inline void EOR_I(struct rp2a03 *rp2a03);
static inline void EOR_IX(struct rp2a03 *rp2a03);
static inline void EOR_IY(struct rp2a03 *rp2a03);
static inline void EOR_ZP(struct rp2a03 *rp2a03);
static inline void EOR_ZPX(struct rp2a03 *rp2a03);
static inline void INC(struct rp2a03 *rp2a03, uint16_t address);
static inline void INC_A(struct rp2a03 *rp2a03);
static inline void INC_AX(struct rp2a03 *rp2a03);
static inline void INC_ZP(struct rp2a03 *rp2a03);
static inline void INC_ZPX(struct rp2a03 *rp2a03);
static inline void INX(struct rp2a03 *rp2a03);
static inline void INY(struct rp2a03 *rp2a03);
static inline void JMP(struct rp2a03 *rp2a03, uint16_t address);
static inline void JMP_A(struct rp2a03 *rp2a03);
static inline void JMP_I(struct rp2a03 *rp2a03);
static inline void JSR(struct rp2a03 *rp2a03);
static inline void LDA(struct rp2a03 *rp2a03, uint8_t b);
static inline void LDA_A(struct rp2a03 *rp2a03);
static inline void LDA_AX(struct rp2a03 *rp2a03);
static inline void LDA_AY(struct rp2a03 *rp2a03);
static inline void LDA_I(struct rp2a03 *rp2a03);
static inline void LDA_IX(struct rp2a03 *rp2a03);
static inline void LDA_IY(struct rp2a03 *rp2a03);
static inline void LDA_ZPX(struct rp2a03 *rp2a03);
static inline void LDA_ZP(struct rp2a03 *rp2a03);
static inline void LDX(struct rp2a03 *rp2a03, uint8_t b);
static inline void LDX_A(struct rp2a03 *rp2a03);
static inline void LDX_AY(struct rp2a03 *rp2a03);
static inline void LDX_I(struct rp2a03 *rp2a03);
static inline void LDX_ZP(struct rp2a03 *rp2a03);
static inline void LDX_ZPY(struct rp2a03 *rp2a03);
static inline void LDY(struct rp2a03 *rp2a03, uint8_t b);
static inline void LDY_A(struct rp2a03 *rp2a03);
static inline void LDY_AX(struct rp2a03 *rp2a03);
static inline void LDY_I(struct rp2a03 *rp2a03);
static inline void LDY_ZP(struct rp2a03 *rp2a03);
static inline void LDY_ZPX(struct rp2a03 *rp2a03);
static inline void LSR_ACC(struct rp2a03 *rp2a03);
static inline void LSR_A(struct rp2a03 *rp2a03);
static inline void LSR_AX(struct rp2a03 *rp2a03);
static inline void LSR_ZP(struct rp2a03 *rp2a03);
static inline void LSR_ZPX(struct rp2a03 *rp2a03);
static inline void NOP(struct rp2a03 *rp2a03);
static inline void NOP_A(struct rp2a03 *rp2a03);
static inline void NOP_D(struct rp2a03 *rp2a03);
static inline void ORA(struct rp2a03 *rp2a03, uint8_t b);
static inline void ORA_A(struct rp2a03 *rp2a03);
static inline void ORA_AX(struct rp2a03 *rp2a03);
static inline void ORA_AY(struct rp2a03 *rp2a03);
static inline void ORA_I(struct rp2a03 *rp2a03);
static inline void ORA_IX(struct rp2a03 *rp2a03);
static inline void ORA_IY(struct rp2a03 *rp2a03);
static inline void ORA_ZP(struct rp2a03 *rp2a03);
static inline void ORA_ZPX(struct rp2a03 *rp2a03);
static inline void PHA(struct rp2a03 *rp2a03);
static inline void PHP(struct rp2a03 *rp2a03);
static inline void PLA(struct rp2a03 *rp2a03);
static inline void PLP(struct rp2a03 *rp2a03);
static inline void ROL_ACC(struct rp2a03 *rp2a03);
static inline void ROL_A(struct rp2a03 *rp2a03);
static inline void ROL_AX(struct rp2a03 *rp2a03);
static inline void ROL_ZP(struct rp2a03 *rp2a03);
static inline void ROL_ZPX(struct rp2a03 *rp2a03);
static inline void ROR_ACC(struct rp2a03 *rp2a03);
static inline void ROR_A(struct rp2a03 *rp2a03);
static inline void ROR_AX(struct rp2a03 *rp2a03);
static inline void ROR_ZP(struct rp2a03 *rp2a03);
static inline void ROR_ZPX(struct rp2a03 *rp2a03);
static inline void RTI(struct rp2a03 *rp2a03);
static inline void RTS(struct rp2a03 *rp2a03);
static inline void SBC_A(struct rp2a03 *rp2a03);
static inline void SBC_AX(struct rp2a03 *rp2a03);
static inline void SBC_AY(struct rp2a03 *rp2a03);
static inline void SBC_I(struct rp2a03 *rp2a03);
static inline void SBC_IX(struct rp2a03 *rp2a03);
static inline void SBC_IY(struct rp2a03 *rp2a03);
static inline void SBC_ZP(struct rp2a03 *rp2a03);
static inline void SBC_ZPX(struct rp2a03 *rp2a03);
static inline void SEC(struct rp2a03 *rp2a03);
static inline void SED(struct rp2a03 *rp2a03);
static inline void SEI(struct rp2a03 *rp2a03);
static inline void STA_A(struct rp2a03 *rp2a03);
static inline void STA_AX(struct rp2a03 *rp2a03);
static inline void STA_AY(struct rp2a03 *rp2a03);
static inline void STA_IX(struct rp2a03 *rp2a03);
static inline void STA_IY(struct rp2a03 *rp2a03);
static inline void STA_ZP(struct rp2a03 *rp2a03);
static inline void STA_ZPX(struct rp2a03 *rp2a03);
static inline void STX_A(struct rp2a03 *rp2a03);
static inline void STX_ZP(struct rp2a03 *rp2a03);
static inline void STX_ZPY(struct rp2a03 *rp2a03);
static inline void STY_A(struct rp2a03 *rp2a03);
static inline void STY_ZP(struct rp2a03 *rp2a03);
static inline void STY_ZPX(struct rp2a03 *rp2a03);
static inline void TAX(struct rp2a03 *rp2a03);
static inline void TAY(struct rp2a03 *rp2a03);
static inline void TSX(struct rp2a03 *rp2a03);
static inline void TXA(struct rp2a03 *rp2a03);
static inline void TXS(struct rp2a03 *rp2a03);
static inline void TYA(struct rp2a03 *rp2a03);

void ADC_A(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readw(rp2a03->bus_id,
		rp2a03->PC));
	uint16_t result = rp2a03->A + b + rp2a03->C;
	rp2a03->C = result >> 8;
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = ((~(rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void ADC_AX(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->X;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	uint16_t result = rp2a03->A + b + rp2a03->C;
	rp2a03->C = result >> 8;
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = ((~(rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void ADC_AY(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->Y;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	uint16_t result = rp2a03->A + b + rp2a03->C;
	rp2a03->C = result >> 8;
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = ((~(rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void ADC_I(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint16_t result = rp2a03->A + b + rp2a03->C;
	rp2a03->C = result >> 8;
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = ((~(rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->n_cycles += 2;
}

void ADC_IX(struct rp2a03 *rp2a03)
{
	uint8_t b = (memory_readb(rp2a03->bus_id, rp2a03->PC++) + rp2a03->X);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address);
	uint16_t result = rp2a03->A + b + rp2a03->C;
	rp2a03->C = result >> 8;
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = ((~(rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->n_cycles += 6;
}

void ADC_IY(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address + rp2a03->Y);
	uint16_t result = rp2a03->A + b + rp2a03->C;
	rp2a03->C = result >> 8;
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = ((~(rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->n_cycles += 5;
}

void ADC_ZP(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	uint16_t result = rp2a03->A + b + rp2a03->C;
	rp2a03->C = result >> 8;
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = ((~(rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->n_cycles += 3;
}

void ADC_ZPX(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, (memory_readb(rp2a03->bus_id,
		rp2a03->PC++) + rp2a03->X) % ZP_SIZE);
	uint16_t result = rp2a03->A + b + rp2a03->C;
	rp2a03->C = result >> 8;
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = ((~(rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->n_cycles += 4;
}

void AND(struct rp2a03 *rp2a03, uint8_t b)
{
	rp2a03->A &= b;
	rp2a03->Z = (rp2a03->A == 0);
	rp2a03->N = ((rp2a03->A & 0x80) != 0);
}

void AND_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	AND(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void AND_AX(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->X;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	AND(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void AND_AY(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->Y;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	AND(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void AND_I(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	AND(rp2a03, b);
	rp2a03->n_cycles += 2;
}

void AND_IX(struct rp2a03 *rp2a03)
{
	uint8_t b = (memory_readb(rp2a03->bus_id, rp2a03->PC++) + rp2a03->X);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address);
	AND(rp2a03, b);
	rp2a03->n_cycles += 6;
}

void AND_IY(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address + rp2a03->Y);
	AND(rp2a03, b);
	rp2a03->n_cycles += 5;
}

void AND_ZP(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	AND(rp2a03, b);
	rp2a03->n_cycles += 3;
}

void AND_ZPX(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, (memory_readb(rp2a03->bus_id,
		rp2a03->PC++) + rp2a03->X) % ZP_SIZE);
	AND(rp2a03, b);
	rp2a03->n_cycles += 4;
}

void ASL_ACC(struct rp2a03 *rp2a03)
{
	rp2a03->C = ((rp2a03->A & 0x80) != 0);
	rp2a03->A <<= 1;
	rp2a03->Z = (rp2a03->A == 0);
	rp2a03->N = ((rp2a03->A & 0x80) != 0);
	rp2a03->n_cycles += 2;
}

void ASL_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	rp2a03->C = ((b & 0x80) != 0);
	b <<= 1;
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 6;
}

void ASL_AX(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->X;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	rp2a03->C = ((b & 0x80) != 0);
	b <<= 1;
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 7;
}

void ASL_ZP(struct rp2a03 *rp2a03)
{
	uint8_t address = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	rp2a03->C = ((b & 0x80) != 0);
	b <<= 1;
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	rp2a03->n_cycles += 5;
}

void ASL_ZPX(struct rp2a03 *rp2a03)
{
	uint8_t address = (memory_readb(rp2a03->bus_id, rp2a03->PC++) +
		rp2a03->X) % ZP_SIZE;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	rp2a03->C = ((b & 0x80) != 0);
	b <<= 1;
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	rp2a03->n_cycles += 6;
}

void BCC(struct rp2a03 *rp2a03)
{
	if (!rp2a03->C) {
		rp2a03->PC += (int8_t)memory_readb(rp2a03->bus_id, rp2a03->PC);
		rp2a03->n_cycles++;
	}
	rp2a03->PC++;
	rp2a03->n_cycles += 2;
}

void BCS(struct rp2a03 *rp2a03)
{
	if (rp2a03->C) {
		rp2a03->PC += (int8_t)memory_readb(rp2a03->bus_id, rp2a03->PC);
		rp2a03->n_cycles++;
	}
	rp2a03->PC++;
	rp2a03->n_cycles += 2;
}

void BEQ(struct rp2a03 *rp2a03)
{
	if (rp2a03->Z) {
		rp2a03->PC += (int8_t)memory_readb(rp2a03->bus_id, rp2a03->PC);
		rp2a03->n_cycles++;
	}
	rp2a03->PC++;
	rp2a03->n_cycles += 2;
}

void BIT(struct rp2a03 *rp2a03, uint8_t b)
{
	rp2a03->Z = ((rp2a03->A & b) == 0);
	rp2a03->V = ((b & 0x40) != 0);
	rp2a03->N = ((b & 0x80) != 0);
}

void BIT_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	BIT(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void BIT_ZP(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	BIT(rp2a03, b);
	rp2a03->n_cycles += 3;
}

void BMI(struct rp2a03 *rp2a03)
{
	if (rp2a03->N) {
		rp2a03->PC += (int8_t)memory_readb(rp2a03->bus_id, rp2a03->PC);
		rp2a03->n_cycles++;
	}
	rp2a03->PC++;
	rp2a03->n_cycles += 2;
}

void BNE(struct rp2a03 *rp2a03)
{
	if (!rp2a03->Z) {
		rp2a03->PC += (int8_t)memory_readb(rp2a03->bus_id, rp2a03->PC);
		rp2a03->n_cycles++;
	}
	rp2a03->PC++;
	rp2a03->n_cycles += 2;
}

void BPL(struct rp2a03 *rp2a03)
{
	if (!rp2a03->N) {
		rp2a03->PC += (int8_t)memory_readb(rp2a03->bus_id, rp2a03->PC);
		rp2a03->n_cycles++;
	}
	rp2a03->PC++;
	rp2a03->n_cycles += 2;
}

void BRK(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->PC >> 8,
		STACK_START + rp2a03->S--);
	memory_writeb(rp2a03->bus_id, rp2a03->PC & 0xFF,
		STACK_START + rp2a03->S--);
	rp2a03->B = 1;
	memory_writeb(rp2a03->bus_id, rp2a03->P, STACK_START + rp2a03->S--);
	rp2a03->I = 1;
	rp2a03->PC = memory_readw(rp2a03->bus_id, INTERRUPT_VECTOR);
	rp2a03->n_cycles += 7;
}

void BVC(struct rp2a03 *rp2a03)
{
	if (!rp2a03->V) {
		rp2a03->PC += (int8_t)memory_readb(rp2a03->bus_id, rp2a03->PC);
		rp2a03->n_cycles++;
	}
	rp2a03->PC++;
	rp2a03->n_cycles += 2;
}

void BVS(struct rp2a03 *rp2a03)
{
	if (rp2a03->V) {
		rp2a03->PC += (int8_t)memory_readb(rp2a03->bus_id, rp2a03->PC);
		rp2a03->n_cycles++;
	}
	rp2a03->PC++;
	rp2a03->n_cycles += 2;
}

void CLC(struct rp2a03 *rp2a03)
{
	rp2a03->C = 0;
	rp2a03->n_cycles += 2;
}

void CLD(struct rp2a03 *rp2a03)
{
	rp2a03->D = 0;
	rp2a03->n_cycles += 2;
}

void CLV(struct rp2a03 *rp2a03)
{
	rp2a03->V = 0;
	rp2a03->n_cycles += 2;
}

void CMP(struct rp2a03 *rp2a03, uint8_t b)
{
	rp2a03->C = (rp2a03->A >= b);
	rp2a03->Z = (rp2a03->A == b);
	rp2a03->N = (((rp2a03->A - b) & 0x80) != 0);
}

void CMP_A(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readw(rp2a03->bus_id,
		rp2a03->PC));
	CMP(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void CMP_AX(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readw(rp2a03->bus_id,
		rp2a03->PC) + rp2a03->X);
	CMP(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void CMP_AY(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readw(rp2a03->bus_id,
		rp2a03->PC) + rp2a03->Y);
	CMP(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 6;
}

void CMP_I(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	CMP(rp2a03, b);
	rp2a03->n_cycles += 2;
}

void CMP_IX(struct rp2a03 *rp2a03)
{
	uint8_t b = (memory_readb(rp2a03->bus_id, rp2a03->PC++) + rp2a03->X);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address);
	CMP(rp2a03, b);
	rp2a03->n_cycles += 6;
}

void CMP_IY(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address + rp2a03->Y);
	CMP(rp2a03, b);
	rp2a03->n_cycles += 5;
}

void CMP_ZP(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	CMP(rp2a03, b);
	rp2a03->n_cycles += 3;
}

void CMP_ZPX(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, (memory_readb(rp2a03->bus_id,
		rp2a03->PC++) + rp2a03->X) % ZP_SIZE);
	CMP(rp2a03, b);
	rp2a03->n_cycles += 4;
}

void CPX(struct rp2a03 *rp2a03, uint8_t b)
{
	rp2a03->C = (rp2a03->X >= b);
	rp2a03->Z = (rp2a03->X == b);
	rp2a03->N = (((rp2a03->X - b) & 0x80) != 0);
}

void CPX_A(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readw(rp2a03->bus_id,
		rp2a03->PC));
	CPX(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void CPX_I(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	CPX(rp2a03, b);
	rp2a03->n_cycles += 2;
}

void CPX_ZP(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	CPX(rp2a03, b);
	rp2a03->n_cycles += 3;
}

void CPY(struct rp2a03 *rp2a03, uint8_t b)
{
	rp2a03->C = (rp2a03->Y >= b);
	rp2a03->Z = (rp2a03->Y == b);
	rp2a03->N = (((rp2a03->Y - b) & 0x80) != 0);
}

void CPY_A(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readw(rp2a03->bus_id,
		rp2a03->PC));
	CPY(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void CPY_I(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	CPY(rp2a03, b);
	rp2a03->n_cycles += 2;
}

void CPY_ZP(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	CPY(rp2a03, b);
	rp2a03->n_cycles += 3;
}

void DEC(struct rp2a03 *rp2a03, uint16_t address)
{
	uint8_t b = memory_readb(rp2a03->bus_id, address) - 1;
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	memory_writeb(rp2a03->bus_id, b, address);
}

void DEC_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	DEC(rp2a03, address);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 6;
}

void DEC_AX(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->X;
	DEC(rp2a03, address);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 7;
}

void DEC_ZP(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	DEC(rp2a03, address);
	rp2a03->n_cycles += 5;
}

void DEC_ZPX(struct rp2a03 *rp2a03)
{
	uint16_t address = (memory_readb(rp2a03->bus_id, rp2a03->PC++) +
		rp2a03->X) % ZP_SIZE;
	DEC(rp2a03, address);
	rp2a03->n_cycles += 6;
}

void DEX(struct rp2a03 *rp2a03)
{
	rp2a03->X--;
	rp2a03->Z = (rp2a03->X == 0);
	rp2a03->N = ((rp2a03->X & 0x80) != 0);
	rp2a03->n_cycles += 2;
}

void DEY(struct rp2a03 *rp2a03)
{
	rp2a03->Y--;
	rp2a03->Z = (rp2a03->Y == 0);
	rp2a03->N = ((rp2a03->Y & 0x80) != 0);
	rp2a03->n_cycles += 2;
}

void EOR(struct rp2a03 *rp2a03, uint8_t b)
{
	rp2a03->A ^= b;
	rp2a03->Z = (rp2a03->A == 0);
	rp2a03->N = ((rp2a03->A & 0x80) != 0);
}

void EOR_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	EOR(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void EOR_AX(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->X;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	EOR(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void EOR_AY(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->Y;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	EOR(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void EOR_I(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	EOR(rp2a03, b);
	rp2a03->n_cycles += 2;
}

void EOR_IX(struct rp2a03 *rp2a03)
{
	uint8_t b = (memory_readb(rp2a03->bus_id, rp2a03->PC++) + rp2a03->X);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address);
	EOR(rp2a03, b);
	rp2a03->n_cycles += 6;
}

void EOR_IY(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address + rp2a03->Y);
	EOR(rp2a03, b);
	rp2a03->n_cycles += 5;
}

void EOR_ZP(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	EOR(rp2a03, b);
	rp2a03->n_cycles += 3;
}

void EOR_ZPX(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, (memory_readb(rp2a03->bus_id,
		rp2a03->PC++) + rp2a03->X) % ZP_SIZE);
	EOR(rp2a03, b);
	rp2a03->n_cycles += 4;
}

void INC(struct rp2a03 *rp2a03, uint16_t address)
{
	uint8_t b = memory_readb(rp2a03->bus_id, address) + 1;
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	memory_writeb(rp2a03->bus_id, b, address);
}

void INC_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	INC(rp2a03, address);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 6;
}

void INC_AX(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->X;
	INC(rp2a03, address);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 7;
}

void INC_ZP(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	INC(rp2a03, address);
	rp2a03->n_cycles += 5;
}

void INC_ZPX(struct rp2a03 *rp2a03)
{
	uint16_t address = (memory_readb(rp2a03->bus_id, rp2a03->PC++) +
		rp2a03->X) % ZP_SIZE;
	INC(rp2a03, address);
	rp2a03->n_cycles += 6;
}

void INX(struct rp2a03 *rp2a03)
{
	rp2a03->X++;
	rp2a03->Z = (rp2a03->X == 0);
	rp2a03->N = ((rp2a03->X & 0x80) != 0);
	rp2a03->n_cycles += 2;
}

void INY(struct rp2a03 *rp2a03)
{
	rp2a03->Y++;
	rp2a03->Z = (rp2a03->Y == 0);
	rp2a03->N = ((rp2a03->Y & 0x80) != 0);
	rp2a03->n_cycles += 2;
}

void JMP(struct rp2a03 *rp2a03, uint16_t address)
{
	rp2a03->PC = address;
}

void JMP_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	JMP(rp2a03, address);
	rp2a03->n_cycles += 3;
}

void JMP_I(struct rp2a03 *rp2a03)
{
	uint16_t address_1 = memory_readw(rp2a03->bus_id, rp2a03->PC);
	uint16_t address_2 = ((address_1 + 1) & 0xFF) | (address_1 & 0xFF00);
	uint16_t address = memory_readb(rp2a03->bus_id, address_1) |
		(memory_readb(rp2a03->bus_id, address_2) << 8);
	JMP(rp2a03, address);
	rp2a03->n_cycles += 5;
}

void JSR(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, (rp2a03->PC + 1) >> 8,
		STACK_START + rp2a03->S--);
	memory_writeb(rp2a03->bus_id, (rp2a03->PC + 1) & 0xFF,
		STACK_START + rp2a03->S--);
	rp2a03->PC = memory_readw(rp2a03->bus_id, rp2a03->PC);
	rp2a03->n_cycles += 6;
}

void LDA(struct rp2a03 *rp2a03, uint8_t b)
{
	rp2a03->A = b;
	rp2a03->Z = (rp2a03->A == 0);
	rp2a03->N = ((rp2a03->A & 0x80) != 0);
}

void LDA_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	LDA(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void LDA_AX(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->X;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	LDA(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void LDA_AY(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->Y;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	LDA(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void LDA_I(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	LDA(rp2a03, b);
	rp2a03->n_cycles += 2;
}

void LDA_IX(struct rp2a03 *rp2a03)
{
	uint8_t b = (memory_readb(rp2a03->bus_id, rp2a03->PC++) + rp2a03->X);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address);
	LDA(rp2a03, b);
	rp2a03->n_cycles += 5;
}

void LDA_IY(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address + rp2a03->Y);
	LDA(rp2a03, b);
	rp2a03->n_cycles += 5;
}

void LDA_ZPX(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, (memory_readb(rp2a03->bus_id,
		rp2a03->PC++) + rp2a03->X) % ZP_SIZE);
	LDA(rp2a03, b);
	rp2a03->n_cycles += 4;
}

void LDA_ZP(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	LDA(rp2a03, b);
	rp2a03->n_cycles += 3;
}

void LDX(struct rp2a03 *rp2a03, uint8_t b)
{
	rp2a03->X = b;
	rp2a03->Z = (rp2a03->X == 0);
	rp2a03->N = ((rp2a03->X & 0x80) != 0);
}

void LDX_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	LDX(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void LDX_AY(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->Y;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	LDX(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void LDX_I(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	LDX(rp2a03, b);
	rp2a03->n_cycles += 2;
}

void LDX_ZP(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	LDX(rp2a03, b);
	rp2a03->n_cycles += 3;
}

void LDX_ZPY(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, (memory_readb(rp2a03->bus_id,
		rp2a03->PC++) + rp2a03->Y) % ZP_SIZE);
	LDX(rp2a03, b);
	rp2a03->n_cycles += 4;
}

void LDY(struct rp2a03 *rp2a03, uint8_t b)
{
	rp2a03->Y = b;
	rp2a03->Z = (rp2a03->Y == 0);
	rp2a03->N = ((rp2a03->Y & 0x80) != 0);
}

void LDY_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	LDY(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void LDY_AX(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->X;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	LDY(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void LDY_I(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	LDY(rp2a03, b);
	rp2a03->n_cycles += 2;
}

void LDY_ZP(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	LDY(rp2a03, b);
	rp2a03->n_cycles += 3;
}

void LDY_ZPX(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, (memory_readb(rp2a03->bus_id,
		rp2a03->PC++) + rp2a03->X) % ZP_SIZE);
	LDY(rp2a03, b);
	rp2a03->n_cycles += 4;
}

void LSR_ACC(struct rp2a03 *rp2a03)
{
	rp2a03->C = ((rp2a03->A & 0x01) != 0);
	rp2a03->A >>= 1;
	rp2a03->Z = (rp2a03->A == 0);
	rp2a03->N = 0;
	rp2a03->n_cycles += 2;
}

void LSR_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	rp2a03->C = ((b & 0x01) != 0);
	b >>= 1;
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = 0;
	rp2a03->PC += 2;
	rp2a03->n_cycles += 6;
}

void LSR_AX(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->X;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	rp2a03->C = ((b & 0x01) != 0);
	b >>= 1;
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = 0;
	rp2a03->PC += 2;
	rp2a03->n_cycles += 7;
}

void LSR_ZP(struct rp2a03 *rp2a03)
{
	uint8_t address = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	rp2a03->C = ((b & 0x01) != 0);
	b >>= 1;
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = 0;
	rp2a03->n_cycles += 5;
}

void LSR_ZPX(struct rp2a03 *rp2a03)
{
	uint8_t address = (memory_readb(rp2a03->bus_id, rp2a03->PC++) +
		rp2a03->X) % ZP_SIZE;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	rp2a03->C = ((b & 0x01) != 0);
	b >>= 1;
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = 0;
	rp2a03->n_cycles += 6;
}

void NOP(struct rp2a03 *rp2a03)
{
	rp2a03->n_cycles += 2;
}

void NOP_A(struct rp2a03 *rp2a03)
{
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void NOP_D(struct rp2a03 *rp2a03)
{
	rp2a03->PC++;
	rp2a03->n_cycles += 3;
}

void ORA(struct rp2a03 *rp2a03, uint8_t b)
{
	rp2a03->A |= b;
	rp2a03->Z = (rp2a03->A == 0);
	rp2a03->N = ((rp2a03->A & 0x80) != 0);
}

void ORA_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	ORA(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void ORA_AX(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->X;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	ORA(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void ORA_AY(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->Y;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	ORA(rp2a03, b);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void ORA_I(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	ORA(rp2a03, b);
	rp2a03->n_cycles += 2;
}

void ORA_IX(struct rp2a03 *rp2a03)
{
	uint8_t b = (memory_readb(rp2a03->bus_id, rp2a03->PC++) + rp2a03->X);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address);
	ORA(rp2a03, b);
	rp2a03->n_cycles += 6;
}

void ORA_IY(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address + rp2a03->Y);
	ORA(rp2a03, b);
	rp2a03->n_cycles += 5;
}

void ORA_ZP(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	ORA(rp2a03, b);
	rp2a03->n_cycles += 3;
}

void ORA_ZPX(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, (memory_readb(rp2a03->bus_id,
		rp2a03->PC++) + rp2a03->X) % ZP_SIZE);
	ORA(rp2a03, b);
	rp2a03->n_cycles += 4;
}

void PHA(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->A, STACK_START + rp2a03->S--);
	rp2a03->n_cycles += 3;
}

void PHP(struct rp2a03 *rp2a03)
{
	rp2a03->B = 1;
	memory_writeb(rp2a03->bus_id, rp2a03->P, STACK_START + rp2a03->S--);
	rp2a03->B = 0;
	rp2a03->n_cycles += 3;
}

void PLA(struct rp2a03 *rp2a03)
{
	rp2a03->A = memory_readb(rp2a03->bus_id, STACK_START + ++rp2a03->S);
	rp2a03->Z = (rp2a03->A == 0);
	rp2a03->N = ((rp2a03->A & 0x80) != 0);
	rp2a03->n_cycles += 4;
}

void PLP(struct rp2a03 *rp2a03)
{
	rp2a03->P = memory_readb(rp2a03->bus_id, STACK_START + ++rp2a03->S);
	rp2a03->unused = 1;
	rp2a03->B = 0;
	rp2a03->n_cycles += 4;
}

void ROL_ACC(struct rp2a03 *rp2a03)
{
	uint8_t old_carry = rp2a03->C;
	rp2a03->C = ((rp2a03->A & 0x80) != 0);
	rp2a03->A = (rp2a03->A << 1) | old_carry;
	rp2a03->Z = (rp2a03->A == 0);
	rp2a03->N = ((rp2a03->A & 0x80) != 0);
	rp2a03->n_cycles += 2;
}

void ROL_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	uint8_t old_carry = rp2a03->C;
	rp2a03->C = ((b & 0x80) != 0);
	b = (b << 1) | old_carry;
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 6;
}

void ROL_AX(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->X;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	uint8_t old_carry = rp2a03->C;
	rp2a03->C = ((b & 0x80) != 0);
	b = (b << 1) | old_carry;
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 7;
}

void ROL_ZP(struct rp2a03 *rp2a03)
{
	uint8_t address = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	uint8_t old_carry = rp2a03->C;
	rp2a03->C = ((b & 0x80) != 0);
	b = (b << 1) | old_carry;
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	rp2a03->n_cycles += 5;
}

void ROL_ZPX(struct rp2a03 *rp2a03)
{
	uint8_t address = (memory_readb(rp2a03->bus_id, rp2a03->PC++) +
		rp2a03->X) % ZP_SIZE;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	uint8_t old_carry = rp2a03->C;
	rp2a03->C = ((b & 0x80) != 0);
	b = (b << 1) | old_carry;
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	rp2a03->n_cycles += 6;
}

void ROR_ACC(struct rp2a03 *rp2a03)
{
	uint8_t old_carry = rp2a03->C;
	rp2a03->C = ((rp2a03->A & 0x01) != 0);
	rp2a03->A = (rp2a03->A >> 1) | (old_carry << 7);
	rp2a03->Z = (rp2a03->A == 0);
	rp2a03->N = ((rp2a03->A & 0x80) != 0);
	rp2a03->n_cycles += 2;
}

void ROR_A(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	uint8_t old_carry = rp2a03->C;
	rp2a03->C = ((b & 0x01) != 0);
	b = (b >> 1) | (old_carry << 7);
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 6;
}

void ROR_AX(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id, rp2a03->PC) + rp2a03->X;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	uint8_t old_carry = rp2a03->C;
	rp2a03->C = ((b & 0x01) != 0);
	b = (b >> 1) | (old_carry << 7);
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 7;
}

void ROR_ZP(struct rp2a03 *rp2a03)
{
	uint8_t address = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	uint8_t old_carry = rp2a03->C;
	rp2a03->C = ((b & 0x01) != 0);
	b = (b >> 1) | (old_carry << 7);
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	rp2a03->n_cycles += 5;
}

void ROR_ZPX(struct rp2a03 *rp2a03)
{
	uint8_t address = (memory_readb(rp2a03->bus_id, rp2a03->PC++) +
		rp2a03->X) % ZP_SIZE;
	uint8_t b = memory_readb(rp2a03->bus_id, address);
	uint8_t old_carry = rp2a03->C;
	rp2a03->C = ((b & 0x01) != 0);
	b = (b >> 1) | (old_carry << 7);
	memory_writeb(rp2a03->bus_id, b, address);
	rp2a03->Z = (b == 0);
	rp2a03->N = ((b & 0x80) != 0);
	rp2a03->n_cycles += 6;
}

void RTI(struct rp2a03 *rp2a03)
{
	rp2a03->P = memory_readb(rp2a03->bus_id, STACK_START + ++rp2a03->S);
	rp2a03->unused = 1;
	rp2a03->B = 0;
	rp2a03->PC = memory_readb(rp2a03->bus_id, STACK_START + ++rp2a03->S);
	rp2a03->PC |= memory_readb(rp2a03->bus_id, STACK_START +
		++rp2a03->S) << 8;
	rp2a03->n_cycles += 6;
}

void RTS(struct rp2a03 *rp2a03)
{
	uint16_t PC = memory_readb(rp2a03->bus_id, STACK_START + ++rp2a03->S);
	PC |= memory_readb(rp2a03->bus_id, STACK_START + ++rp2a03->S) << 8;
	rp2a03->PC = PC + 1;
	rp2a03->n_cycles += 6;
}

void SBC_A(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readw(rp2a03->bus_id,
		rp2a03->PC));
	int16_t result = rp2a03->A - b - (1 - rp2a03->C);
	rp2a03->C = ~(result >> 8);
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = (((rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void SBC_AX(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readw(rp2a03->bus_id,
		rp2a03->PC) + rp2a03->X);
	int16_t result = rp2a03->A - b - (1 - rp2a03->C);
	rp2a03->C = ~(result >> 8);
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = (((rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void SBC_AY(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readw(rp2a03->bus_id,
		rp2a03->PC) + rp2a03->Y);
	int16_t result = rp2a03->A - b - (1 - rp2a03->C);
	rp2a03->C = ~(result >> 8);
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = (((rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void SBC_I(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	int16_t result = rp2a03->A - b - (1 - rp2a03->C);
	rp2a03->C = ~(result >> 8);
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = (((rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->n_cycles += 2;
}

void SBC_IX(struct rp2a03 *rp2a03)
{
	uint8_t b = (memory_readb(rp2a03->bus_id, rp2a03->PC++) + rp2a03->X);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address);
	int16_t result = rp2a03->A - b - (1 - rp2a03->C);
	rp2a03->C = ~(result >> 8);
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = (((rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->n_cycles += 6;
}

void SBC_IY(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, rp2a03->PC++);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	b = memory_readb(rp2a03->bus_id, address + rp2a03->Y);
	int16_t result = rp2a03->A - b - (1 - rp2a03->C);
	rp2a03->C = ~(result >> 8);
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = (((rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->n_cycles += 5;
}

void SBC_ZP(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	int16_t result = rp2a03->A - b - (1 - rp2a03->C);
	rp2a03->C = ~(result >> 8);
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = (((rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->n_cycles += 3;
}

void SBC_ZPX(struct rp2a03 *rp2a03)
{
	uint8_t b = memory_readb(rp2a03->bus_id, (memory_readb(rp2a03->bus_id,
		rp2a03->PC++) + rp2a03->X) % ZP_SIZE);
	int16_t result = rp2a03->A - b - (1 - rp2a03->C);
	rp2a03->C = ~(result >> 8);
	rp2a03->Z = ((uint8_t)result == 0);
	rp2a03->V = (((rp2a03->A ^ b) & (rp2a03->A ^ result) & 0x80) != 0);
	rp2a03->N = ((result & 0x80) != 0);
	rp2a03->A = result;
	rp2a03->n_cycles += 4;
}

void SEC(struct rp2a03 *rp2a03)
{
	rp2a03->C = 1;
	rp2a03->n_cycles += 2;
}

void SED(struct rp2a03 *rp2a03)
{
	rp2a03->D = 1;
	rp2a03->n_cycles += 2;
}

void SEI(struct rp2a03 *rp2a03)
{
	rp2a03->I = 1;
	rp2a03->n_cycles += 2;
}

void STA_A(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->A, memory_readw(rp2a03->bus_id,
		rp2a03->PC));
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void STA_AX(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->A, memory_readw(rp2a03->bus_id,
		rp2a03->PC) + rp2a03->X);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 5;
}

void STA_AY(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->A, memory_readw(rp2a03->bus_id,
		rp2a03->PC) + rp2a03->Y);
	rp2a03->PC += 2;
	rp2a03->n_cycles += 5;
}

void STA_IX(struct rp2a03 *rp2a03)
{
	uint8_t b = (memory_readb(rp2a03->bus_id, rp2a03->PC++) + rp2a03->X);
	uint16_t address = memory_readb(rp2a03->bus_id, b % ZP_SIZE) |
		(memory_readb(rp2a03->bus_id, (b + 1) % ZP_SIZE) << 8);
	memory_writeb(rp2a03->bus_id, rp2a03->A, address);
	rp2a03->n_cycles += 6;
}

void STA_IY(struct rp2a03 *rp2a03)
{
	uint16_t address = memory_readw(rp2a03->bus_id,
		memory_readb(rp2a03->bus_id, rp2a03->PC++)) + rp2a03->Y;
	memory_writeb(rp2a03->bus_id, rp2a03->A, address);
	rp2a03->n_cycles += 6;
}

void STA_ZP(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->A, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	rp2a03->n_cycles += 3;
}

void STA_ZPX(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->A, (memory_readb(rp2a03->bus_id,
		rp2a03->PC++) + rp2a03->X) % ZP_SIZE);
	rp2a03->n_cycles += 4;
}

void STX_A(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->X, memory_readw(rp2a03->bus_id,
		rp2a03->PC));
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void STX_ZP(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->X, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	rp2a03->n_cycles += 3;
}

void STX_ZPY(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->X, (memory_readb(rp2a03->bus_id,
		rp2a03->PC++) + rp2a03->Y) % ZP_SIZE);
	rp2a03->n_cycles += 4;
}

void STY_A(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->Y, memory_readw(rp2a03->bus_id,
		rp2a03->PC));
	rp2a03->PC += 2;
	rp2a03->n_cycles += 4;
}

void STY_ZP(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->Y, memory_readb(rp2a03->bus_id,
		rp2a03->PC++));
	rp2a03->n_cycles += 3;
}

void STY_ZPX(struct rp2a03 *rp2a03)
{
	memory_writeb(rp2a03->bus_id, rp2a03->Y, (memory_readb(rp2a03->bus_id,
		rp2a03->PC++) + rp2a03->X) % ZP_SIZE);
	rp2a03->n_cycles += 4;
}

void TAX(struct rp2a03 *rp2a03)
{
	rp2a03->X = rp2a03->A;
	rp2a03->Z = (rp2a03->X == 0);
	rp2a03->N = ((rp2a03->X & 0x80) != 0);
	rp2a03->n_cycles += 2;
}

void TAY(struct rp2a03 *rp2a03)
{
	rp2a03->Y = rp2a03->A;
	rp2a03->Z = (rp2a03->Y == 0);
	rp2a03->N = ((rp2a03->Y & 0x80) != 0);
	rp2a03->n_cycles += 2;
}

void TSX(struct rp2a03 *rp2a03)
{
	rp2a03->X = rp2a03->S;
	rp2a03->Z = (rp2a03->X == 0);
	rp2a03->N = ((rp2a03->X & 0x80) != 0);
	rp2a03->n_cycles += 2;
}

void TXA(struct rp2a03 *rp2a03)
{
	rp2a03->A = rp2a03->X;
	rp2a03->Z = (rp2a03->A == 0);
	rp2a03->N = ((rp2a03->A & 0x80) != 0);
	rp2a03->n_cycles += 2;
}

void TXS(struct rp2a03 *rp2a03)
{
	rp2a03->S = rp2a03->X;
	rp2a03->n_cycles += 2;
}

void TYA(struct rp2a03 *rp2a03)
{
	rp2a03->A = rp2a03->Y;
	rp2a03->Z = (rp2a03->A == 0);
	rp2a03->N = ((rp2a03->A & 0x80) != 0);
	rp2a03->n_cycles += 2;
}

void rp2a03_tick(clock_data_t *data)
{
	struct rp2a03 *rp2a03 = data;
	uint8_t opcode;

	/* Decrement number of cycles left to execute instruction */
	rp2a03->n_cycles--;

	/* Return if instruction is still in progress */
	if (rp2a03->n_cycles > 0)
		return;

	/* Fetch opcode */
	opcode = memory_readb(rp2a03->bus_id, rp2a03->PC++);

	/* Execute opcode */
	switch (opcode) {
	case 0x00:
		return BRK(rp2a03);
	case 0x01:
		return ORA_IX(rp2a03);
	case 0x04:
		return NOP_D(rp2a03);
	case 0x05:
		return ORA_ZP(rp2a03);
	case 0x06:
		return ASL_ZP(rp2a03);
	case 0x08:
		return PHP(rp2a03);
	case 0x09:
		return ORA_I(rp2a03);
	case 0x0A:
		return ASL_ACC(rp2a03);
	case 0x0C:
		return NOP_A(rp2a03);
	case 0x0D:
		return ORA_A(rp2a03);
	case 0x0E:
		return ASL_A(rp2a03);
	case 0x10:
		return BPL(rp2a03);
	case 0x11:
		return ORA_IY(rp2a03);
	case 0x15:
		return ORA_ZPX(rp2a03);
	case 0x16:
		return ASL_ZPX(rp2a03);
	case 0x18:
		return CLC(rp2a03);
	case 0x19:
		return ORA_AY(rp2a03);
	case 0x1D:
		return ORA_AX(rp2a03);
	case 0x1E:
		return ASL_AX(rp2a03);
	case 0x20:
		return JSR(rp2a03);
	case 0x21:
		return AND_IX(rp2a03);
	case 0x24:
		return BIT_ZP(rp2a03);
	case 0x25:
		return AND_ZP(rp2a03);
	case 0x26:
		return ROL_ZP(rp2a03);
	case 0x28:
		return PLP(rp2a03);
	case 0x29:
		return AND_I(rp2a03);
	case 0x2A:
		return ROL_ACC(rp2a03);
	case 0x2C:
		return BIT_A(rp2a03);
	case 0x2D:
		return AND_A(rp2a03);
	case 0x2E:
		return ROL_A(rp2a03);
	case 0x30:
		return BMI(rp2a03);
	case 0x31:
		return AND_IY(rp2a03);
	case 0x35:
		return AND_ZPX(rp2a03);
	case 0x36:
		return ROL_ZPX(rp2a03);
	case 0x38:
		return SEC(rp2a03);
	case 0x39:
		return AND_AY(rp2a03);
	case 0x3D:
		return AND_AX(rp2a03);
	case 0x3E:
		return ROL_AX(rp2a03);
	case 0x40:
		return RTI(rp2a03);
	case 0x41:
		return EOR_IX(rp2a03);
	case 0x44:
		return NOP_D(rp2a03);
	case 0x45:
		return EOR_ZP(rp2a03);
	case 0x46:
		return LSR_ZP(rp2a03);
	case 0x48:
		return PHA(rp2a03);
	case 0x49:
		return EOR_I(rp2a03);
	case 0x4A:
		return LSR_ACC(rp2a03);
	case 0x4C:
		return JMP_A(rp2a03);
	case 0x4D:
		return EOR_A(rp2a03);
	case 0x4E:
		return LSR_A(rp2a03);
	case 0x50:
		return BVC(rp2a03);
	case 0x51:
		return EOR_IY(rp2a03);
	case 0x55:
		return EOR_ZPX(rp2a03);
	case 0x56:
		return LSR_ZPX(rp2a03);
	case 0x59:
		return EOR_AY(rp2a03);
	case 0x5D:
		return EOR_AX(rp2a03);
	case 0x5E:
		return LSR_AX(rp2a03);
	case 0x60:
		return RTS(rp2a03);
	case 0x61:
		return ADC_IX(rp2a03);
	case 0x64:
		return NOP_D(rp2a03);
	case 0x65:
		return ADC_ZP(rp2a03);
	case 0x66:
		return ROR_ZP(rp2a03);
	case 0x68:
		return PLA(rp2a03);
	case 0x69:
		return ADC_I(rp2a03);
	case 0x6A:
		return ROR_ACC(rp2a03);
	case 0x6C:
		return JMP_I(rp2a03);
	case 0x6D:
		return ADC_A(rp2a03);
	case 0x6E:
		return ROR_A(rp2a03);
	case 0x70:
		return BVS(rp2a03);
	case 0x71:
		return ADC_IY(rp2a03);
	case 0x75:
		return ADC_ZPX(rp2a03);
	case 0x76:
		return ROR_ZPX(rp2a03);
	case 0x78:
		return SEI(rp2a03);
	case 0x79:
		return ADC_AY(rp2a03);
	case 0x7D:
		return ADC_AX(rp2a03);
	case 0x7E:
		return ROR_AX(rp2a03);
	case 0x81:
		return STA_IX(rp2a03);
	case 0x84:
		return STY_ZP(rp2a03);
	case 0x85:
		return STA_ZP(rp2a03);
	case 0x86:
		return STX_ZP(rp2a03);
	case 0x88:
		return DEY(rp2a03);
	case 0x8A:
		return TXA(rp2a03);
	case 0x8C:
		return STY_A(rp2a03);
	case 0x8D:
		return STA_A(rp2a03);
	case 0x8E:
		return STX_A(rp2a03);
	case 0x90:
		return BCC(rp2a03);
	case 0x91:
		return STA_IY(rp2a03);
	case 0x94:
		return STY_ZPX(rp2a03);
	case 0x95:
		return STA_ZPX(rp2a03);
	case 0x96:
		return STX_ZPY(rp2a03);
	case 0x98:
		return TYA(rp2a03);
	case 0x99:
		return STA_AY(rp2a03);
	case 0x9A:
		return TXS(rp2a03);
	case 0x9D:
		return STA_AX(rp2a03);
	case 0xA0:
		return LDY_I(rp2a03);
	case 0xA1:
		return LDA_IX(rp2a03);
	case 0xA2:
		return LDX_I(rp2a03);
	case 0xA4:
		return LDY_ZP(rp2a03);
	case 0xA5:
		return LDA_ZP(rp2a03);
	case 0xA6:
		return LDX_ZP(rp2a03);
	case 0xA8:
		return TAY(rp2a03);
	case 0xA9:
		return LDA_I(rp2a03);
	case 0xAC:
		return LDY_A(rp2a03);
	case 0xAD:
		return LDA_A(rp2a03);
	case 0xAE:
		return LDX_A(rp2a03);
	case 0xAA:
		return TAX(rp2a03);
	case 0xB0:
		return BCS(rp2a03);
	case 0xB1:
		return LDA_IY(rp2a03);
	case 0xB4:
		return LDY_ZPX(rp2a03);
	case 0xB5:
		return LDA_ZPX(rp2a03);
	case 0xB6:
		return LDX_ZPY(rp2a03);
	case 0xB8:
		return CLV(rp2a03);
	case 0xB9:
		return LDA_AY(rp2a03);
	case 0xBA:
		return TSX(rp2a03);
	case 0xBC:
		return LDY_AX(rp2a03);
	case 0xBD:
		return LDA_AX(rp2a03);
	case 0xBE:
		return LDX_AY(rp2a03);
	case 0xC0:
		return CPY_I(rp2a03);
	case 0xC1:
		return CMP_IX(rp2a03);
	case 0xC4:
		return CPY_ZP(rp2a03);
	case 0xC5:
		return CMP_ZP(rp2a03);
	case 0xC6:
		return DEC_ZP(rp2a03);
	case 0xC8:
		return INY(rp2a03);
	case 0xC9:
		return CMP_I(rp2a03);
	case 0xCA:
		return DEX(rp2a03);
	case 0xCC:
		return CPY_A(rp2a03);
	case 0xCD:
		return CMP_A(rp2a03);
	case 0xCE:
		return DEC_A(rp2a03);
	case 0xD0:
		return BNE(rp2a03);
	case 0xD1:
		return CMP_IY(rp2a03);
	case 0xD5:
		return CMP_ZPX(rp2a03);
	case 0xD6:
		return DEC_ZPX(rp2a03);
	case 0xD8:
		return CLD(rp2a03);
	case 0xD9:
		return CMP_AY(rp2a03);
	case 0xDD:
		return CMP_AX(rp2a03);
	case 0xDE:
		return DEC_AX(rp2a03);
	case 0xE0:
		return CPX_I(rp2a03);
	case 0xE1:
		return SBC_IX(rp2a03);
	case 0xE4:
		return CPX_ZP(rp2a03);
	case 0xE5:
		return SBC_ZP(rp2a03);
	case 0xE6:
		return INC_ZP(rp2a03);
	case 0xE8:
		return INX(rp2a03);
	case 0xE9:
		return SBC_I(rp2a03);
	case 0xEA:
		return NOP(rp2a03);
	case 0xEC:
		return CPX_A(rp2a03);
	case 0xED:
		return SBC_A(rp2a03);
	case 0xEE:
		return INC_A(rp2a03);
	case 0xF0:
		return BEQ(rp2a03);
	case 0xF1:
		return SBC_IY(rp2a03);
	case 0xF5:
		return SBC_ZPX(rp2a03);
	case 0xF6:
		return INC_ZPX(rp2a03);
	case 0xF8:
		return SED(rp2a03);
	case 0xF9:
		return SBC_AY(rp2a03);
	case 0xFD:
		return SBC_AX(rp2a03);
	case 0xFE:
		return INC_AX(rp2a03);
	default:
		fprintf(stderr, "rp2a03: unknown opcode (%02x)!\n", opcode);
		break;
	}
}

bool rp2a03_init(struct cpu_instance *instance)
{
	struct rp2a03 *rp2a03;
	struct resource *clk = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);

	/* Allocate rp2a03 structure and set private data */
	rp2a03 = malloc(sizeof(struct rp2a03));
	instance->priv_data = rp2a03;

	/* Initialize registers and processor data */
	rp2a03->bus_id = instance->bus_id;
	rp2a03->PC = memory_readw(rp2a03->bus_id, RESET_VECTOR);
	rp2a03->I = 1;
	rp2a03->unused = 1;
	rp2a03->n_cycles = 1;

	/* Add CPU clock */
	rp2a03->clock.rate = clk->rate;
	rp2a03->clock.data = rp2a03;
	rp2a03->clock.tick = rp2a03_tick;
	clock_add(&rp2a03->clock);

	return true;
}

void rp2a03_deinit(struct cpu_instance *instance)
{
	struct rp2a03 *rp2a03 = instance->priv_data;
	free(rp2a03);
}

CPU_START(rp2a03)
	.init = rp2a03_init,
	.deinit = rp2a03_deinit
CPU_END

