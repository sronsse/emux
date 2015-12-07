#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bitops.h>
#include <clock.h>
#include <controller.h>
#include <cpu.h>
#include <memory.h>
#include <util.h>

#define NUM_REGISTERS		30
#define MADR(n)			(0 + n * 4)
#define BCR(n)			(1 + n * 4)
#define CHCR(n)			(2 + n * 4)
#define DPCR			28
#define DICR			29

#define MADR_CHANNEL(a)		((a - 0) / 4)
#define BCR_CHANNEL(a)		((a - 1) / 4)
#define CHCR_CHANNEL(a)		((a - 2) / 4)

#define NUM_CHANNELS		7
#define TRANSFER_TO_RAM		0
#define TRANSFER_FROM_RAM	1
#define STEP_FORWARD		0
#define STEP_BACKWARD		1
#define MODE_TRANSFER_ALL	0
#define MODE_SYNC_TO_REQUESTS	1
#define MODE_LINKED_LIST	2
#define CH6_END_OF_LIST_HEADER	0xFFFFFF

#define DPCR_PRIORITY(dpcr, n) \
	bitops_getl(dpcr, 4 * n, 3)
#define DPCR_MASTER_ENABLE(dpcr, n) \
	bitops_getl(dpcr, 4 * n + 3, 1)

struct madr {
	uint32_t address:24;
	uint32_t unused:8;
};

union bcr {
	struct {
		uint32_t num_words:16;
		uint32_t unused:16;
	};
	struct {
		uint32_t block_size:16;
		uint32_t amount:16;
	};
};

struct chcr {
	uint32_t transfer_dir:1;
	uint32_t mem_address_step:1;
	uint32_t unused:6;
	uint32_t chopping_enable:1;
	uint32_t sync_mode:2;
	uint32_t unused2:5;
	uint32_t chopping_dma_win_size:3;
	uint32_t unused3:1;
	uint32_t chopping_cpu_win_size:3;
	uint32_t unused4:1;
	uint32_t start_busy:1;
	uint32_t unused5:3;
	uint32_t start_trigger:1;
	uint32_t pause:1;
	uint32_t unused6:2;
};

struct dpcr {
	uint32_t dma0_priority:3;
	uint32_t dma0_master_enable:1;
	uint32_t dma1_priority:3;
	uint32_t dma1_master_enable:1;
	uint32_t dma2_priority:3;
	uint32_t dma2_master_enable:1;
	uint32_t dma3_priority:3;
	uint32_t dma3_master_enable:1;
	uint32_t dma4_priority:3;
	uint32_t dma4_master_enable:1;
	uint32_t dma5_priority:3;
	uint32_t dma5_master_enable:1;
	uint32_t dma6_priority:3;
	uint32_t dma6_master_enable:1;
	uint32_t unused:4;
};

struct dicr {
	uint32_t unused:6;
	uint32_t unused2:9;
	uint32_t force_irq:1;
	uint32_t irq_enable:7;
	uint32_t irq_master_enable:1;
	uint32_t irq_flags:7;
	uint32_t irq_master_flag:1;
};

union dma_registers {
	uint32_t raw[NUM_REGISTERS];
	struct {
		struct madr madr0;
		union bcr bcr0;
		struct chcr chcr0;
		uint32_t reserved0;
		struct madr madr1;
		union bcr bcr1;
		struct chcr chcr1;
		uint32_t reserved1;
		struct madr madr2;
		union bcr bcr2;
		struct chcr chcr2;
		uint32_t reserved2;
		struct madr madr3;
		union bcr bcr3;
		struct chcr chcr3;
		uint32_t reserved3;
		struct madr madr4;
		union bcr bcr4;
		struct chcr chcr4;
		uint32_t reserved4;
		struct madr madr5;
		union bcr bcr5;
		struct chcr chcr5;
		uint32_t reserved5;
		struct madr madr6;
		union bcr bcr6;
		struct chcr chcr6;
		uint32_t reserved6;
		struct dpcr dpcr;
		struct dicr dicr;
	};
};

union linked_list_header {
	uint32_t raw;
	struct {
		uint32_t address:21;
		uint32_t unused2:2;
		uint32_t stop_bit:1;
		uint32_t num_words:8;
	};
};

struct channel {
	bool active;
};

struct psx_dma {
	union dma_registers registers;
	struct channel channels[NUM_CHANNELS];
	int irq;
	int bus_id;
	struct region region;
	struct clock clock;
};

static bool psx_dma_init(struct controller_instance *instance);
static void psx_dma_reset(struct controller_instance *instance);
static void psx_dma_deinit(struct controller_instance *instance);
static uint32_t psx_dma_readl(struct psx_dma *psx_dma, address_t a);
static void psx_dma_writel(struct psx_dma *psx_dma, uint32_t l, address_t a);
static void psx_dma_tick(struct psx_dma *psx_dma);
static void psx_dma_start_transfer(struct psx_dma *psx_dma, int ch);
static void psx_dma_end_transfer(struct psx_dma *psx_dma, int ch);
static void psx_dma_mode_0(struct psx_dma *psx_dma, int ch);
static void psx_dma_mode_1(struct psx_dma *psx_dma, int ch);
static void psx_dma_mode_2(struct psx_dma *psx_dma, int ch);
static void psx_dma_ch6_quirk(struct psx_dma *psx_dma);

static struct mops psx_dma_mops = {
	.readl = (readl_t)psx_dma_readl,
	.writel = (writel_t)psx_dma_writel
};

uint32_t psx_dma_readl(struct psx_dma *psx_dma, address_t a)
{
	int index;

	/* Get register index from raw address */
	index = a / sizeof(uint32_t);

	/* Return requested register */
	return psx_dma->registers.raw[index];
}

void psx_dma_writel(struct psx_dma *psx_dma, uint32_t l, address_t a)
{
	struct chcr *chcr;
	struct dicr *dicr;
	int index;
	bool f;

	/* Get register index from raw address */
	index = a / sizeof(uint32_t);

	/* Write requested register */
	psx_dma->registers.raw[index] = l;

	/* Handle write */
	switch (index) {
	case CHCR(0):
	case CHCR(1):
	case CHCR(2):
	case CHCR(3):
	case CHCR(4):
	case CHCR(5):
	case CHCR(6):
		/* Start transfer if requested */
		chcr = (struct chcr *)&psx_dma->registers.raw[index];
		if (chcr->start_busy)
			psx_dma_start_transfer(psx_dma, CHCR_CHANNEL(index));
		break;
	case DICR:
		/* Acknowledge interrupts if any */
		dicr = (struct dicr *)&l;
		psx_dma->registers.dicr.irq_flags &= ~dicr->irq_flags;

		/* Update master flag */
		dicr = &psx_dma->registers.dicr;
		f = ((dicr->irq_enable & dicr->irq_flags) != 0);
		f &= dicr->irq_master_enable;
		f |= dicr->force_irq;
		dicr->irq_master_flag = f;
		break;
	}
}

void psx_dma_start_transfer(struct psx_dma *psx_dma, int ch)
{
	struct chcr *chcr;

	/* Return already if channel is disabled */
	if (!DPCR_MASTER_ENABLE(&psx_dma->registers.raw[DPCR], ch))
		return;

	/* Flag channel as active */
	psx_dma->channels[ch].active = true;

	/* The Start/Trigger bit is automatically cleared upon
	begin of the transfer, this bit needs to be set only in
	SyncMode = 0 */
	chcr = (struct chcr *)&psx_dma->registers.raw[CHCR(ch)];
	if (chcr->sync_mode == MODE_TRANSFER_ALL)
		chcr->start_trigger = 0;

	/* Make sure main clock is enabled and halt CPU */
	psx_dma->clock.enabled = true;
	cpu_halt(true);
}

void psx_dma_end_transfer(struct psx_dma *psx_dma, int ch)
{
	struct chcr *chcr;
	struct dicr *dicr;

	/* Indicate transfer completion */
	chcr = (struct chcr *)&psx_dma->registers.raw[CHCR(ch)];
	chcr->start_busy = 0;

	/* Get DICR register pointer */
	dicr = &psx_dma->registers.dicr;

	/* Set channel interrupt status bit if enabled */
	if (dicr->irq_enable & (1 << ch))
		dicr->irq_flags |= (1 << ch);

	/* Return if IRQ master flag is already set */
	if (dicr->irq_master_flag)
		return;

	/* Set IRQ master flag */
	dicr->irq_master_flag = ((dicr->irq_enable & dicr->irq_flags) != 0);
	dicr->irq_master_flag &= dicr->irq_master_enable;
	dicr->irq_master_flag |= dicr->force_irq;

	/* Interrupt CPU if needed */
	if (dicr->irq_master_flag)
		cpu_interrupt(psx_dma->irq);
}

void psx_dma_ch6_quirk(struct psx_dma *psx_dma)
{
	address_t address;
	uint32_t word;
	int num_words;
	int dir;
	int i;

	/* Set initial DMA address */
	address = psx_dma->registers.madr6.address;

	/* Set step direction */
	dir = (psx_dma->registers.chcr6.mem_address_step == STEP_FORWARD) ?
		1 : -1;

	/* Get number of words (0 = 0x10000 words) */
	num_words = psx_dma->registers.bcr6.num_words;
	if (num_words == 0)
		num_words = 0x10000;

	/* Transfer words (computed from destination address and step) */
	for (i = 0; i < num_words - 1; i++) {
		word = address + dir * sizeof(uint32_t);
		memory_writel(psx_dma->bus_id, word, address);
		address = word;
	}

	/* Finalize list */
	memory_writel(psx_dma->bus_id, CH6_END_OF_LIST_HEADER, address);

	/* Consume one clock per word */
	clock_consume(num_words);

	/* Flag channel as inactive */
	psx_dma->channels[6].active = false;
}

void psx_dma_mode_0(struct psx_dma *psx_dma, int ch)
{
	struct madr *madr;
	union bcr *bcr;
	struct chcr *chcr;
	address_t address;
	uint32_t word;
	int num_words;
	int i;

	/* Handle special channel 6 scenario */
	if (ch == 6) {
		psx_dma_ch6_quirk(psx_dma);
		return;
	}

	/* Set initial DMA address */
	madr = (struct madr *)&psx_dma->registers.raw[MADR(ch)];
	address = madr->address;

	/* Get number of words (0 = 0x10000 words) */
	bcr = (union bcr *)&psx_dma->registers.raw[BCR(ch)];
	num_words = bcr->num_words;
	if (num_words == 0)
		num_words = 0x10000;

	/* Transfer words */
	chcr = (struct chcr *)&psx_dma->registers.raw[CHCR(ch)];
	for (i = 0; i < num_words; i++) {
		switch (chcr->transfer_dir) {
		case TRANSFER_TO_RAM:
			word = dma_readl(ch);
			memory_writel(psx_dma->bus_id, word, address);
			break;
		case TRANSFER_FROM_RAM:
			word = memory_readl(psx_dma->bus_id, address);
			dma_writel(ch, word);
			break;
		}

		/* Increment/decrement address based on step */
		address += (chcr->mem_address_step == STEP_FORWARD) ?
			sizeof(uint32_t) : -sizeof(uint32_t);
	}

	/* Flag channel as inactive */
	psx_dma->channels[ch].active = false;
}

void psx_dma_mode_1(struct psx_dma *psx_dma, int ch)
{
	struct madr *madr;
	union bcr *bcr;
	struct chcr *chcr;
	address_t address;
	uint32_t word;
	int num_words;
	int i;

	/* Set initial DMA address */
	madr = (struct madr *)&psx_dma->registers.raw[MADR(ch)];
	address = madr->address;

	/* Get number of words */
	bcr = (union bcr *)&psx_dma->registers.raw[BCR(ch)];
	num_words = (uint32_t)bcr->amount * (uint32_t)bcr->block_size;

	/* Transfer words */
	chcr = (struct chcr *)&psx_dma->registers.raw[CHCR(ch)];
	for (i = 0; i < num_words; i++) {
		switch (chcr->transfer_dir) {
		case TRANSFER_TO_RAM:
			word = dma_readl(ch);
			memory_writel(psx_dma->bus_id, word, address);
			break;
		case TRANSFER_FROM_RAM:
			word = memory_readl(psx_dma->bus_id, address);
			dma_writel(ch, word);
			break;
		}

		/* Increment/decrement address based on step */
		address += (chcr->mem_address_step == STEP_FORWARD) ?
			sizeof(uint32_t) : -sizeof(uint32_t);
	}

	/* Flag channel as inactive */
	psx_dma->channels[ch].active = false;
}

void psx_dma_mode_2(struct psx_dma *psx_dma, int ch)
{
	struct madr *madr;
	struct chcr *chcr;
	union linked_list_header header;
	address_t address;
	uint32_t word;
	int i;

	/* Set initial DMA address */
	madr = (struct madr *)&psx_dma->registers.raw[MADR(ch)];
	address = madr->address;

	/* Run until linked list tail is reached */
	chcr = (struct chcr *)&psx_dma->registers.raw[CHCR(ch)];
	do {
		/* Get header */
		switch (chcr->transfer_dir) {
		case TRANSFER_TO_RAM:
			header.raw = dma_readl(ch);
			break;
		case TRANSFER_FROM_RAM:
			header.raw = memory_readl(psx_dma->bus_id, address);
			break;
		}

		/* Transfer words */
		for (i = 0; i < header.num_words; i++) {
			/* Increment address */
			address += sizeof(uint32_t);

			/* Read and write word */
			switch (chcr->transfer_dir) {
			case TRANSFER_TO_RAM:
				word = dma_readl(ch);
				memory_writel(psx_dma->bus_id, word, address);
				break;
			case TRANSFER_FROM_RAM:
				word = memory_readl(psx_dma->bus_id, address);
				dma_writel(ch, word);
				break;
			}
		}

		/* Update address */
		address = header.address;
	} while (!header.stop_bit);

	/* Flag channel as inactive */
	psx_dma->channels[ch].active = false;
}

void psx_dma_tick(struct psx_dma *psx_dma)
{
	struct chcr *chcr;
	bool busy;
	int i;

	/* Loop through all busy channels */
	busy = false;
	for (i = 0; i < NUM_CHANNELS; i++) {
		/* Handle channel if needed */
		chcr = (struct chcr *)&psx_dma->registers.raw[CHCR(i)];
		if (chcr->start_busy) {
			/* Handle transfer completion if needed */
			if (!psx_dma->channels[i].active) {
				psx_dma_end_transfer(psx_dma, i);
				continue;
			}

			/* Handle channel based on sync mode */
			switch (chcr->sync_mode) {
			case MODE_TRANSFER_ALL:
				psx_dma_mode_0(psx_dma, i);
				break;
			case MODE_SYNC_TO_REQUESTS:
				psx_dma_mode_1(psx_dma, i);
				break;
			case MODE_LINKED_LIST:
				psx_dma_mode_2(psx_dma, i);
				break;
			}

			/* Save busy flag */
			busy = true;
		}
	}

	/* Disable main clock and unhalt CPU if no channel is busy */
	if (!busy) {
		psx_dma->clock.enabled = false;
		cpu_halt(false);
	}
}

bool psx_dma_init(struct controller_instance *instance)
{
	struct psx_dma *psx_dma;
	struct resource *res;

	/* Allocate psx_dma structure */
	instance->priv_data = calloc(1, sizeof(struct psx_dma));
	psx_dma = instance->priv_data;

	/* Save bus ID for later use */
	psx_dma->bus_id = instance->bus_id;

	/* Set up psx_dma memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	psx_dma->region.area = res;
	psx_dma->region.mops = &psx_dma_mops;
	psx_dma->region.data = psx_dma;
	memory_region_add(&psx_dma->region);

	/* Save IRQ */
	res = resource_get("irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	psx_dma->irq = res->data.irq;

	/* Initialize clocks */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	psx_dma->clock.rate = res->data.clk;
	psx_dma->clock.data = psx_dma;
	psx_dma->clock.tick = (clock_tick_t)psx_dma_tick;
	clock_add(&psx_dma->clock);

	return true;
}

void psx_dma_reset(struct controller_instance *instance)
{
	struct psx_dma *psx_dma = instance->priv_data;

	/* Reset registers */
	memset(psx_dma->registers.raw, 0, NUM_REGISTERS * sizeof(uint32_t));

	/* Reset channel info */
	memset(psx_dma->channels, 0, NUM_CHANNELS * sizeof(struct channel));

	/* Disable clock */
	psx_dma->clock.enabled = false;
}

void psx_dma_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(psx_dma)
	.init = psx_dma_init,
	.reset = psx_dma_reset,
	.deinit = psx_dma_deinit
CONTROLLER_END

