#include <string.h>
#include <bitops.h>
#include <clock.h>
#include <controller.h>
#include <cpu.h>
#include <env.h>
#include <memory.h>
#include <util.h>

#define INDEX_OFFSET		4
#define INDEX			0
#define STATUS			0
#define COMMAND			(1 + 0 * INDEX_OFFSET)
#define SOUND_MAP_DATA		(1 + 1 * INDEX_OFFSET)
#define SOUND_MAP_INFO		(1 + 2 * INDEX_OFFSET)
#define VOL_RIGHT_RIGHT		(1 + 3 * INDEX_OFFSET)
#define RESP_FIFO_1		(1 + 0 * INDEX_OFFSET)
#define RESP_FIFO_2		(1 + 1 * INDEX_OFFSET)
#define RESP_FIFO_3		(1 + 2 * INDEX_OFFSET)
#define RESP_FIFO_4		(1 + 3 * INDEX_OFFSET)
#define PARAM_FIFO		(2 + 0 * INDEX_OFFSET)
#define INT_ENABLE_W		(2 + 1 * INDEX_OFFSET)
#define VOL_LEFT_LEFT		(2 + 2 * INDEX_OFFSET)
#define VOL_RIGHT_LEFT		(2 + 3 * INDEX_OFFSET)
#define DATA_FIFO_1		(2 + 0 * INDEX_OFFSET)
#define DATA_FIFO_2		(2 + 1 * INDEX_OFFSET)
#define DATA_FIFO_3		(2 + 2 * INDEX_OFFSET)
#define DATA_FIFO_4		(2 + 3 * INDEX_OFFSET)
#define REQUEST			(3 + 0 * INDEX_OFFSET)
#define INT_FLAG_RW		(3 + 1 * INDEX_OFFSET)
#define VOL_LEFT_RIGHT		(3 + 2 * INDEX_OFFSET)
#define VOL_APPLY		(3 + 3 * INDEX_OFFSET)
#define INT_ENABLE_R_1		(3 + 0 * INDEX_OFFSET)
#define INT_ENABLE_R_2		(3 + 2 * INDEX_OFFSET)
#define INT_FLAG_R		(3 + 3 * INDEX_OFFSET)

#define RESP_FIFO_SIZE		16
#define PARAM_FIFO_SIZE		16
#define DATA_FIFO_SIZE		KB(4)
#define SRAM_SIZE		KB(32)

#define CYCLES_1_DEF_NORMAL	0x0000C4E1
#define CYCLES_1_DEF_STOPPED	0x00005CF4
#define CYCLES_1_INIT		0x00013CCE
#define CYCLES_2_GETID		0x00004A00
#define CYCLES_2_PAUSE_SINGLE	0x0021181C
#define CYCLES_2_PAUSE_DOUBLE	0x0010BD93
#define CYCLES_2_PAUSE_PAUSED	0x00001DF2
#define CYCLES_2_STOP_SINGLE	0x00D38ACA
#define CYCLES_2_STOP_DOUBLE	0x018A6076
#define CYCLES_2_STOP_PAUSED	0x00001D7B
#define CYCLES_READ_SINGLE	0x0006E1CD
#define CYCLES_READ_DOUBLE	0x00036CD2

#define STATE_NORMAL		0x00
#define STATE_READ		0x01
#define STATE_SEEK		0x02
#define STATE_PLAY		0x04

#define ERR_NO_ERROR		0x00
#define ERR_INVALID_PARAM	0x10
#define ERR_WRONG_NUM_PARAMS	0x20
#define ERR_INVALID_CMD		0x40
#define ERR_CANNOT_RESPOND	0x80
#define ERR_SEEK_FAILED		0x04
#define ERR_DRIVE_OPENED	0x08

#define FROM_BCD(v)		(((v) & 0x0F) + ((v) >> 4) * 10)
#define TO_BCD(v)		(((v) % 10) | (((v) / 10) << 4))
#define SECS_PER_MIN		60
#define SECTORS_PER_SEC		75

#define REGION_BYTE_US		0x41
#define REGION_BYTE_EU		0x45
#define REGION_BYTE_JP		0x49

union index_status {
	uint8_t raw;
	struct {
		uint8_t index:2;
		uint8_t xa_adpcm_fifo_empty:1;
		uint8_t param_fifo_empty:1;
		uint8_t param_fifo_full:1;
		uint8_t resp_fifo_empty:1;
		uint8_t data_fifo_empty:1;
		uint8_t transmission_busy:1;
	};
};

union request {
	uint8_t raw;
	struct {
		uint8_t unused:5;
		uint8_t command_start_int:1;
		uint8_t unused2:1;
		uint8_t want_data:1;
	};
};

union int_enable {
	uint8_t raw;
	struct {
		uint8_t bits:5;
		uint8_t unknown:3;
	};
};

union int_flag_w {
	uint8_t raw;
	struct {
		uint8_t ack_int1_int7:3;
		uint8_t ack_int8:1;
		uint8_t ack_int10:1;
		uint8_t unknown:1;
		uint8_t reset_param_fifo:1;
		uint8_t unknown2:1;
	};
};

union int_flag_r {
	uint8_t raw;
	struct {
		uint8_t resp_received:3;
		uint8_t unknown:1;
		uint8_t cmd_start:1;
		uint8_t unknown2:3;
	};
};

union stat {
	uint8_t raw;
	struct {
		uint8_t error:1;
		uint8_t spindle_motor:1;
		uint8_t seek_error:1;
		uint8_t id_error:1;
		uint8_t shell_open:1;
		uint8_t state_bits:3;
	};
};

union mode {
	uint8_t raw;
	struct {
		uint8_t cdda:1;
		uint8_t auto_pause:1;
		uint8_t report:1;
		uint8_t xa_filter:1;
		uint8_t ignore_bit:1;
		uint8_t sector_size:1;
		uint8_t xa_adpcm:1;
		uint8_t speed:1;
	};
};

enum resp_int {
	INT0,
	INT1,
	INT2,
	INT3,
	INT4,
	INT5
};

enum drive_status {
	DRIVE_DOOR_OPEN,
	DRIVE_SPIN_UP,
	DRIVE_DETECT_BUSY,
	DRIVE_NO_DISC,
	DRIVE_AUDIO_DISC,
	DRIVE_UNLICENSED_MODE1,
	DRIVE_UNLICENSED_MODE2,
	DRIVE_UNLICENSED_MODE2_AUDIO,
	DRIVE_LICENSED_MODE2,
	DRIVE_MODCHIP_AUDIO_MODE1
};

struct fifo {
	uint8_t *data;
	int size;
	int pos;
	int num;
};

struct cmd {
	uint8_t code;
	enum resp_int resp;
	int step;
	bool complete;
	uint8_t pending;
};

struct sram {
	uint8_t data[SRAM_SIZE];
	int size;
};

struct pos {
	int track;
	int index;
	int mm;
	int ss;
	int sect;
	int amm;
	int ass;
	int asect;
};

struct psx_cdrom {
	union index_status index_status;
	union int_enable int_enable;
	union int_flag_r int_flag;
	union stat stat;
	union mode mode;
	enum drive_status drive_status;
	uint8_t region_byte;
	uint32_t loc;
	bool reading;
	struct cmd cmd;
	struct fifo resp_fifo;
	struct fifo param_fifo;
	struct fifo data_fifo;
	struct sram sram;
	struct pos pos;
	struct region region;
	int irq;
};

static bool psx_cdrom_init(struct controller_instance *instance);
static void psx_cdrom_reset(struct controller_instance *instance);
static void psx_cdrom_deinit(struct controller_instance *instance);
static uint8_t psx_cdrom_readb(struct psx_cdrom *cdrom, address_t a);
static void psx_cdrom_writeb(struct psx_cdrom *cdrom, uint8_t b, address_t a);

static bool fifo_empty(struct fifo *fifo);
static bool fifo_full(struct fifo *fifo);
static bool fifo_enqueue(struct fifo *fifo, uint8_t data);
static bool fifo_dequeue(struct fifo *fifo, uint8_t *data);
static void fifo_reset(struct fifo *fifo);
static bool resp_fifo_enqueue(struct psx_cdrom *cdrom, uint8_t data);
static bool resp_fifo_dequeue(struct psx_cdrom *cdrom, uint8_t *data);
static void resp_fifo_reset(struct psx_cdrom *cdrom);
static bool param_fifo_enqueue(struct psx_cdrom *cdrom, uint8_t data);
static bool param_fifo_dequeue(struct psx_cdrom *cdrom, uint8_t *data);
static void param_fifo_reset(struct psx_cdrom *cdrom);
static bool data_fifo_enqueue(struct psx_cdrom *cdrom, uint8_t data);
static bool data_fifo_dequeue(struct psx_cdrom *cdrom, uint8_t *data);
static void data_fifo_reset(struct psx_cdrom *cdrom);

static struct mops psx_cdrom_mops = {
	.readb = (readb_t)psx_cdrom_readb,
	.writeb = (writeb_t)psx_cdrom_writeb
};

uint8_t psx_cdrom_readb(struct psx_cdrom *cdrom, address_t address)
{
	uint8_t b = 0;

	/* Adapt address based on index */
	if (address > 0)
		address += cdrom->index_status.index * INDEX_OFFSET;

	/* Read requested register */
	switch (address) {
	case STATUS:
		/* Read status register */
		b = cdrom->index_status.raw;
		break;
	case RESP_FIFO_1:
	case RESP_FIFO_2:
	case RESP_FIFO_3:
	case RESP_FIFO_4:
		/* Dequeue response FIFO */
		resp_fifo_dequeue(cdrom, &b);
		break;
	case INT_ENABLE_R_1:
	case INT_ENABLE_R_2:
		/* Read interrupt enable register */
		b = cdrom->int_enable.raw;
		break;
	case INT_FLAG_RW:
	case INT_FLAG_R:
		/* Read interrupt flag register */
		b = cdrom->int_flag.raw;
		break;
	case DATA_FIFO_1:
	case DATA_FIFO_2:
	case DATA_FIFO_3:
	case DATA_FIFO_4:
	default:
		/* Dequeue byte from data FIFO */
		data_fifo_dequeue(cdrom, &b);
		break;
	}

	/* Return register */
	return b;
}

void psx_cdrom_writeb(struct psx_cdrom *cdrom, uint8_t b, address_t address)
{
	union int_flag_w int_flag_w;
	union request request;
	int i;

	/* Adapt address based on index */
	if (address > 0)
		address += cdrom->index_status.index * INDEX_OFFSET;

	/* Write requested register */
	switch (address) {
	case INDEX:
		/* Write index register */
		cdrom->index_status.index = bitops_getb(&b, 0, 2);
		break;
	case COMMAND:
		/* Flag command transmission as busy */
		cdrom->index_status.transmission_busy = 1;

		/* Set command to be executed and enable command clock */
		cdrom->cmd.pending = b;
		break;
	case PARAM_FIFO:
		/* Enqueue parameter FIFO */
		param_fifo_enqueue(cdrom, b);
		break;
	case REQUEST:
		/* Adapt data */
		request.raw = b;

		/* Add SRAM contents to data FIFO if requested */
		if (request.want_data) {
			for (i = 0; i < cdrom->sram.size; i++)
				data_fifo_enqueue(cdrom, cdrom->sram.data[i]);
			cdrom->sram.size = 0;
		}

		/* Reset data FIFO if needed */
		if (!request.want_data)
			data_fifo_reset(cdrom);
		break;
	case INT_ENABLE_W:
		/* Write interrupt enable register */
		cdrom->int_enable.bits = bitops_getb(&b, 0, 5);
		break;
	case INT_FLAG_RW:
		/* Adapt data and save previous register */
		int_flag_w.raw = b;

		/* Reset parameter FIFO if requested */
		if (int_flag_w.reset_param_fifo)
			param_fifo_reset(cdrom);

		/* Update interrupt bits */
		cdrom->int_flag.resp_received &= ~int_flag_w.ack_int1_int7;
		cdrom->int_flag.cmd_start &= ~int_flag_w.ack_int10;

		/* Acknowledge interrupts */
		if (int_flag_w.ack_int1_int7) {
			/* Reset response FIFO */
			resp_fifo_reset(cdrom);
		}
		break;
	case SOUND_MAP_DATA:
	case SOUND_MAP_INFO:
	case VOL_RIGHT_RIGHT:
	case VOL_LEFT_LEFT:
	case VOL_RIGHT_LEFT:
	case VOL_LEFT_RIGHT:
	case VOL_APPLY:
	default:
		break;
	}
}

bool fifo_empty(struct fifo *fifo)
{
	/* Return whether FIFO is empty or not */
	return (fifo->num == 0);
}

bool fifo_full(struct fifo *fifo)
{
	/* Return whether FIFO is full or not */
	return (fifo->num == fifo->size);
}

bool fifo_enqueue(struct fifo *fifo, uint8_t data)
{
	/* Return already if FIFO is full */
	if (fifo_full(fifo))
		return false;

	/* Add data to FIFO and handle position overflow */
	fifo->data[fifo->pos++] = data;
	if (fifo->pos == fifo->size)
		fifo->pos = 0;

	/* Increment number of elements */
	fifo->num++;

	/* Return success */
	return true;
}

bool fifo_dequeue(struct fifo *fifo, uint8_t *data)
{
	int index;

	/* Return if FIFO does not have enough elements */
	if (fifo_empty(fifo))
		return false;

	/* Retrieve data from FIFO */
	index = ((fifo->pos - fifo->num) + fifo->size) % fifo->size;
	*data = fifo->data[index];
	fifo->num--;

	/* Return success */
	return true;
}

void fifo_reset(struct fifo *fifo)
{
	/* Reset position and number of elements */
	fifo->pos = 0;
	fifo->num = 0;
}

bool resp_fifo_enqueue(struct psx_cdrom *cdrom, uint8_t data)
{
	bool result;

	/* Enqueue response FIFO */
	result = fifo_enqueue(&cdrom->resp_fifo, data);
	if (!result)
		LOG_W("Could not enqueue response FIFO!\n");

	/* Update empty flag (0 = empty) */
	cdrom->index_status.resp_fifo_empty = 1;

	return result;
}

bool resp_fifo_dequeue(struct psx_cdrom *cdrom, uint8_t *data)
{
	bool result;

	/* Dequeue response FIFO */
	result = fifo_dequeue(&cdrom->resp_fifo, data);
	if (!result)
		LOG_W("Could not dequeue response FIFO!\n");

	/* Update empty flag (0 = empty) */
	cdrom->index_status.resp_fifo_empty = !fifo_empty(&cdrom->resp_fifo);

	return result;
}

void resp_fifo_reset(struct psx_cdrom *cdrom)
{
	/* Reset FIFO and update empty flag (0 = empty) */
	fifo_reset(&cdrom->resp_fifo);
	cdrom->index_status.resp_fifo_empty = 0;
}

bool param_fifo_enqueue(struct psx_cdrom *cdrom, uint8_t data)
{
	bool result;

	/* Enqueue param FIFO */
	result = fifo_enqueue(&cdrom->param_fifo, data);
	if (!result)
		LOG_W("Could not enqueue parameter FIFO!\n");

	/* Update empty/full flags (1 = empty, 0 = full) */
	cdrom->index_status.param_fifo_empty = 0;
	cdrom->index_status.param_fifo_full = !fifo_full(&cdrom->param_fifo);

	return result;
}

bool param_fifo_dequeue(struct psx_cdrom *cdrom, uint8_t *data)
{
	bool result;

	/* Dequeue parameter FIFO */
	result = fifo_dequeue(&cdrom->param_fifo, data);
	if (!result)
		LOG_W("Could not dequeue parameter FIFO!\n");

	/* Update empty/full flags (1 = empty, 0 = full) */
	cdrom->index_status.param_fifo_empty = fifo_empty(&cdrom->param_fifo);
	cdrom->index_status.param_fifo_full = 1;

	return result;
}

void param_fifo_reset(struct psx_cdrom *cdrom)
{
	/* Reset FIFO and update empty/full flags (1 = empty, 0 = full) */
	fifo_reset(&cdrom->param_fifo);
	cdrom->index_status.param_fifo_empty = 1;
	cdrom->index_status.param_fifo_full = 1;
}

bool data_fifo_enqueue(struct psx_cdrom *cdrom, uint8_t data)
{
	bool result;

	/* Enqueue data FIFO */
	result = fifo_enqueue(&cdrom->data_fifo, data);
	if (!result)
		LOG_W("Could not enqueue data FIFO!\n");

	/* Update empty flag (0 = empty) */
	cdrom->index_status.data_fifo_empty = 1;

	return result;
}

bool data_fifo_dequeue(struct psx_cdrom *cdrom, uint8_t *data)
{
	bool result;

	/* Dequeue data FIFO */
	result = fifo_dequeue(&cdrom->data_fifo, data);
	if (!result)
		LOG_W("Could not dequeue data FIFO!\n");

	/* Update empty flag (0 = empty) */
	cdrom->index_status.data_fifo_empty = !fifo_empty(&cdrom->data_fifo);

	return result;
}

void data_fifo_reset(struct psx_cdrom *cdrom)
{
	/* Reset FIFO and update empty flag (0 = empty) */
	fifo_reset(&cdrom->data_fifo);
	cdrom->index_status.data_fifo_empty = 0;
}

bool psx_cdrom_init(struct controller_instance *instance)
{
	struct psx_cdrom *cdrom;
	struct resource *res;

	/* Allocate private structure */
	instance->priv_data = calloc(1, sizeof(struct psx_cdrom));
	cdrom = instance->priv_data;

	/* Set up memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	cdrom->region.area = res;
	cdrom->region.mops = &psx_cdrom_mops;
	cdrom->region.data = cdrom;
	memory_region_add(&cdrom->region);

	/* Get IRQ */
	res = resource_get("irq",
		RESOURCE_IRQ,
		instance->resources,
		instance->num_resources);
	cdrom->irq = res->data.irq;

	/* Allocate FIFOs */
	cdrom->resp_fifo.data = calloc(RESP_FIFO_SIZE, sizeof(uint8_t));
	cdrom->resp_fifo.size = RESP_FIFO_SIZE;
	cdrom->param_fifo.data = calloc(PARAM_FIFO_SIZE, sizeof(uint8_t));
	cdrom->param_fifo.size = PARAM_FIFO_SIZE;
	cdrom->data_fifo.data = calloc(DATA_FIFO_SIZE, sizeof(uint8_t));
	cdrom->data_fifo.size = DATA_FIFO_SIZE;

	/* Force region to America/NTSC */
	cdrom->region_byte = REGION_BYTE_US;

	return true;
}

void psx_cdrom_reset(struct controller_instance *instance)
{
	struct psx_cdrom *cdrom = instance->priv_data;

	/* Reset internal states */
	cdrom->index_status.index = 0;
	cdrom->index_status.xa_adpcm_fifo_empty = 0;
	cdrom->index_status.transmission_busy = 0;
	cdrom->int_enable.bits = 0;
	cdrom->int_enable.unknown = 0x07;
	cdrom->int_flag.raw = 0;
	cdrom->int_flag.unknown2 = 0x07;
	cdrom->stat.raw = 0;
	cdrom->stat.shell_open = 1;
	cdrom->stat.spindle_motor = 1;
	cdrom->mode.raw = 0;
	cdrom->reading = false;

	/* Reset FIFOs */
	resp_fifo_reset(cdrom);
	param_fifo_reset(cdrom);
	data_fifo_reset(cdrom);

	/* Reset command state */
	cdrom->cmd.code = 0;
	cdrom->cmd.resp = INT0;
	cdrom->cmd.step = 0;
	cdrom->cmd.complete = true;

	/* Reset SRAM */
	cdrom->sram.size = 0;
}

void psx_cdrom_deinit(struct controller_instance *instance)
{
	struct psx_cdrom *cdrom = instance->priv_data;
	free(cdrom->resp_fifo.data);
	free(cdrom->param_fifo.data);
	free(cdrom->data_fifo.data);
	free(instance->priv_data);
}

CONTROLLER_START(psx_cdrom)
	.init = psx_cdrom_init,
	.reset = psx_cdrom_reset,
	.deinit = psx_cdrom_deinit
CONTROLLER_END

