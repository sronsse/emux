#include <stdlib.h>
#include <clock.h>
#include <cpu.h>
#include <log.h>
#include <memory.h>
#include <resource.h>

#define INITIAL_PC	0xBFC00000

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

struct r3051 {
	union instruction instruction;
	uint32_t PC;
	int bus_id;
	struct clock clock;
};

static bool r3051_init(struct cpu_instance *instance);
static void r3051_reset(struct cpu_instance *instance);
static void r3051_deinit(struct cpu_instance *instance);
static void r3051_tick(struct r3051 *cpu);

void r3051_tick(struct r3051 *cpu)
{
	/* Fetch instruction */
	cpu->instruction.raw = memory_readl(cpu->bus_id, cpu->PC);
	cpu->PC += 4;

	/* Execute instruction */
	switch (cpu->instruction.opcode) {
	default:
		LOG_W("Unknown instruction (%08x)!\n", cpu->instruction.raw);
		break;
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

	return true;
}

void r3051_reset(struct cpu_instance *instance)
{
	struct r3051 *cpu = instance->priv_data;

	/* Intialize processor data */
	cpu->PC = INITIAL_PC;

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

