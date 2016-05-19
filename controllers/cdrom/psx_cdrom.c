#include <string.h>
#include <bitops.h>
#include <cdrom.h>
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
	struct dma_channel dma_channel;
	struct clock cmd_clock;
	int irq;
};

static bool psx_cdrom_init(struct controller_instance *instance);
static void psx_cdrom_reset(struct controller_instance *instance);
static void psx_cdrom_deinit(struct controller_instance *instance);
static void psx_cdrom_cmd_tick(struct psx_cdrom *cdrom);
static uint8_t psx_cdrom_readb(struct psx_cdrom *cdrom, address_t a);
static void psx_cdrom_writeb(struct psx_cdrom *cdrom, uint8_t b, address_t a);
static uint32_t psx_cdrom_dma_readl(struct psx_cdrom *cdrom);

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

static void cmd_handle_status(struct psx_cdrom *cdrom, uint8_t error);
static void cmd_get_stat(struct psx_cdrom *cdrom);
static void cmd_set_loc(struct psx_cdrom *cdrom);
static void cmd_readn(struct psx_cdrom *cdrom);
static void cmd_pause(struct psx_cdrom *cdrom);
static void cmd_init(struct psx_cdrom *cdrom);
static void cmd_mute(struct psx_cdrom *cdrom);
static void cmd_demute(struct psx_cdrom *cdrom);
static void cmd_set_filter(struct psx_cdrom *cdrom);
static void cmd_set_mode(struct psx_cdrom *cdrom);
static void cmd_get_locp(struct psx_cdrom *cdrom);
static void cmd_get_tn(struct psx_cdrom *cdrom);
static void cmd_seekl(struct psx_cdrom *cdrom);
static void cmd_test(struct psx_cdrom *cdrom);
static void cmd_get_id(struct psx_cdrom *cdrom);
static void cmd_reads(struct psx_cdrom *cdrom);
static void cmd_read_toc(struct psx_cdrom *cdrom);
static void cmd_handle_timings(struct psx_cdrom *cdrom);

static struct mops psx_cdrom_mops = {
	.readb = (readb_t)psx_cdrom_readb,
	.writeb = (writeb_t)psx_cdrom_writeb
};

static struct dma_ops psx_cdrom_dma_ops = {
	.readl = (dma_readl_t)psx_cdrom_dma_readl
};

void cmd_handle_status(struct psx_cdrom *cdrom, uint8_t error)
{
	/* Set error bit if needed */
	if (error != 0)
		cdrom->stat.error = 1;

	/* Set seek error bit if needed */
	if (error == ERR_SEEK_FAILED)
		cdrom->stat.seek_error = 1;

	/* Enqueue status */
	resp_fifo_enqueue(cdrom, cdrom->stat.raw);

	/* Enqueue error if needed */
	if (error != 0)
		resp_fifo_enqueue(cdrom, error);

	/* Set response type based on error presence */
	cdrom->cmd.resp = (error == 0) ? INT3 : INT5;
}

/* 01h: Getstat -> INT3(stat) */
void cmd_get_stat(struct psx_cdrom *cdrom)
{
	/* Send status */
	cmd_handle_status(cdrom, ERR_NO_ERROR);

	/* If the shell is closed, the ShellOpen flag is automatically reset to
	zero after reading stat with the Getstat command */
	if (cdrom->drive_status != DRIVE_DOOR_OPEN)
		cdrom->stat.shell_open = 0;

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 02h: Setloc amm, ass, asect -> INT3(stat) */
void cmd_set_loc(struct psx_cdrom *cdrom)
{
	struct msf msf;
	uint8_t amm = 0;
	uint8_t ass = 0;
	uint8_t asect = 0;

	/* Handle absence of disc */
	if (cdrom->drive_status == DRIVE_NO_DISC) {
		cmd_handle_status(cdrom, ERR_CANNOT_RESPOND);
		cdrom->cmd.complete = true;
		return;
	}

	/* Retrieve parameters (and handle parameter errors) */
	if (!param_fifo_dequeue(cdrom, &amm) ||
		!param_fifo_dequeue(cdrom, &ass) ||
		!param_fifo_dequeue(cdrom, &asect)) {
		cmd_handle_status(cdrom, ERR_WRONG_NUM_PARAMS);
		cdrom->cmd.complete = true;
		return;
	}

	/* Convert BCD parameters and fill MSF structure */
	msf.m = FROM_BCD(amm);
	msf.s = FROM_BCD(ass);
	msf.f = FROM_BCD(asect);

	/* Update internal location */
	cdrom->loc = cdrom_get_sector_from_msf(&msf);

	/* Send status */
	cmd_handle_status(cdrom, ERR_NO_ERROR);

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 06h: ReadN -> INT3(stat), INT1(stat), datablock */
void cmd_readn(struct psx_cdrom *cdrom)
{
	/* Handle absence of disc */
	if (cdrom->drive_status == DRIVE_NO_DISC) {
		cmd_handle_status(cdrom, ERR_CANNOT_RESPOND);
		cdrom->cmd.complete = true;
		return;
	}

	/* Flag read state */
	cdrom->stat.state_bits = STATE_READ;

	/* Send status */
	cmd_handle_status(cdrom, ERR_NO_ERROR);

	/* Read with retry. The command responds once with "stat, INT3", and
	then it's repeatedly sending "stat, INT1 --> datablock", that is
	continued even after a successful read has occured; use the Pause
	command to terminate the repeated INT1 responses. */
	cdrom->data_clock.enabled = true;

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 09h: Pause -> INT3(stat), INT2(stat) */
void cmd_pause(struct psx_cdrom *cdrom)
{
	/* Handle absence of disc */
	if (cdrom->drive_status == DRIVE_NO_DISC) {
		cmd_handle_status(cdrom, ERR_CANNOT_RESPOND);
		cdrom->cmd.complete = true;
		return;
	}

	/* Handle first response */
	if (cdrom->cmd.step == 1) {
		/* Send status */
		cmd_handle_status(cdrom, ERR_NO_ERROR);

		/* Abort reading and playing */
		cdrom->stat.state_bits = STATE_NORMAL;

		/* Return and handle other responses later */
		return;
	}

	/* Handle second response (sending status) */
	resp_fifo_enqueue(cdrom, cdrom->stat.raw);
	cdrom->cmd.resp = INT2;

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 0Ah: Init -> INT3(late-stat), INT2(stat) */
void cmd_init(struct psx_cdrom *cdrom)
{
	/* Handle first response */
	if (cdrom->cmd.step == 1) {
		/* Send status */
		cmd_handle_status(cdrom, ERR_NO_ERROR);

		/* Multiple effects at once:
		- Sets mode = 00h
		- Activates drive motor
		- Standby
		- Abort all commands */
		cdrom->mode.raw = 0;
		cdrom->stat.spindle_motor = 1;
		cdrom->stat.state_bits = STATE_NORMAL;
		cdrom->cmd.pending = 0;

		/* Return and handle second response later */
		return;
	}

	/* Handle second response (sending status) */
	resp_fifo_enqueue(cdrom, cdrom->stat.raw);
	cdrom->cmd.resp = INT2;

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 0Bh: Mute -> INT3(stat) */
void cmd_mute(struct psx_cdrom *cdrom)
{
	/* Handle absence of disc */
	if (cdrom->drive_status == DRIVE_NO_DISC) {
		cmd_handle_status(cdrom, ERR_CANNOT_RESPOND);
		cdrom->cmd.complete = true;
		return;
	}

	/* Send status */
	cmd_handle_status(cdrom, ERR_NO_ERROR);

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 0Ch: Demute -> INT3(stat) */
void cmd_demute(struct psx_cdrom *cdrom)
{
	/* Handle absence of disc */
	if (cdrom->drive_status == DRIVE_NO_DISC) {
		cmd_handle_status(cdrom, ERR_CANNOT_RESPOND);
		cdrom->cmd.complete = true;
		return;
	}

	/* Send status */
	cmd_handle_status(cdrom, ERR_NO_ERROR);

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 0Dh: Setfilter file, channel -> INT3(stat) */
void cmd_set_filter(struct psx_cdrom *cdrom)
{
	uint8_t file;
	uint8_t channel;

	/* Handle absence of disc */
	if (cdrom->drive_status == DRIVE_NO_DISC) {
		cmd_handle_status(cdrom, ERR_CANNOT_RESPOND);
		cdrom->cmd.complete = true;
		return;
	}

	/* Retrieve parameters (and handle parameter errors) */
	if (!param_fifo_dequeue(cdrom, &file) ||
		!param_fifo_dequeue(cdrom, &channel)) {
		cmd_handle_status(cdrom, ERR_WRONG_NUM_PARAMS);
		cdrom->cmd.complete = true;
		return;
	}

	/* Send status */
	cmd_handle_status(cdrom, ERR_NO_ERROR);

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 0Eh: Setmode mode -> INT3(stat) */
void cmd_set_mode(struct psx_cdrom *cdrom)
{
	uint8_t mode = 0;

	/* Dequeue mode (without error handling) */
	param_fifo_dequeue(cdrom, &mode);

	/* Update mode */
	cdrom->mode.raw = mode;

	/* Send status */
	cmd_handle_status(cdrom, ERR_NO_ERROR);

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 11h: GetLocP -> INT3(track, index, mm, ss, sect, amm, ass, asect) */
void cmd_get_locp(struct psx_cdrom *cdrom)
{
	/* Handle absence of disc */
	if (cdrom->drive_status == DRIVE_NO_DISC) {
		cmd_handle_status(cdrom, ERR_CANNOT_RESPOND);
		cdrom->cmd.complete = true;
		return;
	}

	/* Enqueue position information in BCD format and set response type */
	resp_fifo_enqueue(cdrom, TO_BCD(cdrom->pos.track));
	resp_fifo_enqueue(cdrom, TO_BCD(cdrom->pos.index));
	resp_fifo_enqueue(cdrom, TO_BCD(cdrom->pos.mm));
	resp_fifo_enqueue(cdrom, TO_BCD(cdrom->pos.ss));
	resp_fifo_enqueue(cdrom, TO_BCD(cdrom->pos.sect));
	resp_fifo_enqueue(cdrom, TO_BCD(cdrom->pos.amm));
	resp_fifo_enqueue(cdrom, TO_BCD(cdrom->pos.ass));
	resp_fifo_enqueue(cdrom, TO_BCD(cdrom->pos.asect));
	cdrom->cmd.resp = INT3;

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 13h: GetTN -> INT3(stat, first, last) */
void cmd_get_tn(struct psx_cdrom *cdrom)
{
	uint8_t first;
	uint8_t last;

	/* Send status */
	cmd_handle_status(cdrom, ERR_NO_ERROR);

	/* Get first and last track numbers */
	first = cdrom_get_first_track_num();
	last = cdrom_get_last_track_num();

	/* Enqueue track numbers in BCD format */
	resp_fifo_enqueue(cdrom, TO_BCD(first));
	resp_fifo_enqueue(cdrom, TO_BCD(last));

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 15h: SeekL -> INT3(stat), INT2(stat) */
void cmd_seekl(struct psx_cdrom *cdrom)
{
	/* Handle absence of disc */
	if (cdrom->drive_status == DRIVE_NO_DISC) {
		cmd_handle_status(cdrom, ERR_CANNOT_RESPOND);
		cdrom->cmd.complete = true;
		return;
	}

	/* Flag seek state */
	cdrom->stat.state_bits = STATE_SEEK;

	/* Handle first response */
	if (cdrom->cmd.step == 1) {
		/* Send status */
		cmd_handle_status(cdrom, ERR_NO_ERROR);

		/* Return and handle second response later */
		return;
	}

	/* Handle second response (sending status) */
	resp_fifo_enqueue(cdrom, cdrom->stat.raw);
	cdrom->cmd.resp = INT2;

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 19h: Test sub_function, depends on sub_function */
void cmd_test(struct psx_cdrom *cdrom)
{
	uint8_t sub_func = 0;

	/* Retrieve sub function (and handle parameter error) */
	if (!param_fifo_dequeue(cdrom, &sub_func)) {
		cmd_handle_status(cdrom, ERR_WRONG_NUM_PARAMS);
		cdrom->cmd.complete = true;
		return;
	}

	LOG_D("Processing CD-ROM Test sub function (%02x)\n", sub_func);

	/* Execute requested sub function */
	switch (sub_func) {
	case 0x20:
		/* Enqueue the date (year-month-day, in BCD format) and version
		of the HC05 CD-ROM controller BIOS (latest known date/version is
		01 Feb 1999, version vC3 (b)), then set response type (INT3) */
		resp_fifo_enqueue(cdrom, 0x97);
		resp_fifo_enqueue(cdrom, 0x01);
		resp_fifo_enqueue(cdrom, 0x10);
		resp_fifo_enqueue(cdrom, 0xC2);
		cdrom->cmd.resp = INT3;
		break;
	default:
		/* Handle sub function error */
		cmd_handle_status(cdrom, ERR_INVALID_PARAM);
		LOG_W("Unknown CD-ROM Test sub function (%02x)!\n", sub_func);
		break;
	}

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 1Ah: GetID -> INT3(stat), INT2/5(stat, flg, typ, atip, "SCEx") */
void cmd_get_id(struct psx_cdrom *cdrom)
{
	/* Handle first response */
	if (cdrom->cmd.step == 1) {
		/* First response is based on drive status */
		switch (cdrom->drive_status) {
		case DRIVE_DOOR_OPEN:
			/* Handle door open state and complete command */
			resp_fifo_enqueue(cdrom, 0x11);
			resp_fifo_enqueue(cdrom, 0x80);
			cdrom->cmd.resp = INT5;
			cdrom->cmd.complete = true;
			break;
		case DRIVE_SPIN_UP:
			/* Handle spin-up and complete command */
			resp_fifo_enqueue(cdrom, 0x01);
			resp_fifo_enqueue(cdrom, 0x80);
			cdrom->cmd.resp = INT5;
			cdrom->cmd.complete = true;
			break;
		case DRIVE_DETECT_BUSY:
			/* Handle busy detection and complete command */
			resp_fifo_enqueue(cdrom, 0x03);
			resp_fifo_enqueue(cdrom, 0x80);
			cdrom->cmd.resp = INT5;
			cdrom->cmd.complete = true;
			break;
		default:
			/* Other commands send status */
			cmd_handle_status(cdrom, ERR_NO_ERROR);
			break;
		}

		/* Return and handle second response later */
		return;
	}

	/* Handle second response based on drive status */
	switch (cdrom->drive_status) {
	case DRIVE_NO_DISC:
		/* Handle absence of disc */
		resp_fifo_enqueue(cdrom, 0x08);
		resp_fifo_enqueue(cdrom, 0x40);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		cdrom->cmd.resp = INT5;
		break;
	case DRIVE_AUDIO_DISC:
		/* Handle audio disc */
		resp_fifo_enqueue(cdrom, 0x0A);
		resp_fifo_enqueue(cdrom, 0x90);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		cdrom->cmd.resp = INT5;
		break;
	case DRIVE_UNLICENSED_MODE1:
		/* Handle unlicensed mode 1 disc */
		resp_fifo_enqueue(cdrom, 0x0A);
		resp_fifo_enqueue(cdrom, 0x80);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		cdrom->cmd.resp = INT5;
		break;
	case DRIVE_UNLICENSED_MODE2:
		/* Handle unlicensed mode 2 disc */
		resp_fifo_enqueue(cdrom, 0x0A);
		resp_fifo_enqueue(cdrom, 0x80);
		resp_fifo_enqueue(cdrom, 0x20);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		cdrom->cmd.resp = INT5;
		break;
	case DRIVE_UNLICENSED_MODE2_AUDIO:
		/* Handle unlicensed mode 2 + audio disc */
		resp_fifo_enqueue(cdrom, 0x0A);
		resp_fifo_enqueue(cdrom, 0x90);
		resp_fifo_enqueue(cdrom, 0x20);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		cdrom->cmd.resp = INT5;
		break;
	case DRIVE_LICENSED_MODE2:
		/* Handle licensed mode 2 disc */
		resp_fifo_enqueue(cdrom, 0x02);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x20);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x53);
		resp_fifo_enqueue(cdrom, 0x43);
		resp_fifo_enqueue(cdrom, 0x45);
		resp_fifo_enqueue(cdrom, cdrom->region_byte);
		cdrom->cmd.resp = INT2;
		break;
	case DRIVE_MODCHIP_AUDIO_MODE1:
		/* Handle modchip audio/mode 1 disc */
		resp_fifo_enqueue(cdrom, 0x02);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x00);
		resp_fifo_enqueue(cdrom, 0x53);
		resp_fifo_enqueue(cdrom, 0x43);
		resp_fifo_enqueue(cdrom, 0x45);
		resp_fifo_enqueue(cdrom, 0x45);
		cdrom->cmd.resp = INT2;
		break;
	default:
		break;
	}

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 1Bh: ReadS -> INT3(stat), INT1(stat), datablock */
void cmd_reads(struct psx_cdrom *cdrom)
{
	/* Handle absence of disc */
	if (cdrom->drive_status == DRIVE_NO_DISC) {
		cmd_handle_status(cdrom, ERR_CANNOT_RESPOND);
		cdrom->cmd.complete = true;
		return;
	}

	/* Flag read state */
	cdrom->stat.state_bits = STATE_READ;

	/* Send status */
	cmd_handle_status(cdrom, ERR_NO_ERROR);

	/* Read without automatic retry. Maybe intended for continous streaming
	video output (to skip bad frames, rather than to interrupt the stream by
	performing read retries). */
	cdrom->data_clock.enabled = true;

	/* Complete command */
	cdrom->cmd.complete = true;
}

/* 1Eh: ReadTOC -> INT3(late-stat), INT2(stat) */
void cmd_read_toc(struct psx_cdrom *cdrom)
{
	/* Handle first response */
	if (cdrom->cmd.step == 1) {
		/* Send status */
		cmd_handle_status(cdrom, ERR_NO_ERROR);

		/* Return and handle second response later */
		return;
	}

	/* Handle second response (sending status) */
	resp_fifo_enqueue(cdrom, cdrom->stat.raw);
	cdrom->cmd.resp = INT2;

	/* Complete command */
	cdrom->cmd.complete = true;
}

void cmd_handle_timings(struct psx_cdrom *cdrom)
{
	/* Handle delay before first response */
	if (cdrom->cmd.step == 0) {
		/* Handle Init and ReadTOC command timings */
		if ((cdrom->cmd.code == 0x0A) || (cdrom->cmd.code == 0x1E)) {
			clock_consume(CYCLES_1_INIT);
			return;
		}

		/* Other command timings depend on motor state */
		clock_consume(cdrom->stat.spindle_motor ?
			CYCLES_1_DEF_NORMAL : CYCLES_1_DEF_STOPPED);
		return;
	}

	/* Handle further response timings based on code */
	switch (cdrom->cmd.code) {
	case 0x09:
		/* Handle Pause timings based on speed */
		clock_consume((cdrom->mode.speed == 0) ?
			CYCLES_2_PAUSE_SINGLE : CYCLES_2_PAUSE_DOUBLE);
		break;
	case 0x1A:
	default:
		/* Handle GetID and other command timings */
		clock_consume(CYCLES_2_GETID);
		clock_consume(CYCLES_2_GETID);
		break;
	}
}

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
		cdrom->cmd_clock.enabled = true;
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

			/* Re-schedule command clock if needed */
			if (!cdrom->cmd.complete || (cdrom->cmd.pending != 0))
				cdrom->cmd_clock.enabled = true;
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

uint32_t psx_cdrom_dma_readl(struct psx_cdrom *cdrom)
{
	uint32_t data;
	uint8_t b = 0;

	/* BIOS default is 24 clks, for some reason most games change it to 40
	clks. TODO: implement register look up */
	clock_consume(24);

	/* Dequeue 4 bytes from data FIFO, combine them, and return data */
	data_fifo_dequeue(cdrom, &b);
	data = b;
	data_fifo_dequeue(cdrom, &b);
	data |= b << 8;
	data_fifo_dequeue(cdrom, &b);
	data |= b << 16;
	data_fifo_dequeue(cdrom, &b);
	data |= b << 24;
	return data;
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

void psx_cdrom_execute_cmd(struct psx_cdrom *cdrom)
{
	LOG_D("Processing CD-ROM command %02x (step %u)\n",
		cdrom->cmd.code,
		cdrom->cmd.step);

	/* Execute command once initialization is complete */
	if (cdrom->cmd.step > 0)
		switch (cdrom->cmd.code) {
		case 0x01:
			cmd_get_stat(cdrom);
			break;
		case 0x02:
			cmd_set_loc(cdrom);
			break;
		case 0x06:
			cmd_readn(cdrom);
			break;
		case 0x09:
			cmd_pause(cdrom);
			break;
		case 0x0A:
			cmd_init(cdrom);
			break;
		case 0x0B:
			cmd_mute(cdrom);
			break;
		case 0x0C:
			cmd_demute(cdrom);
			break;
		case 0x0D:
			cmd_set_filter(cdrom);
			break;
		case 0x0E:
			cmd_set_mode(cdrom);
			break;
		case 0x11:
			cmd_get_locp(cdrom);
			break;
		case 0x13:
			cmd_get_tn(cdrom);
			break;
		case 0x15:
			cmd_seekl(cdrom);
			break;
		case 0x19:
			cmd_test(cdrom);
			break;
		case 0x1A:
			cmd_get_id(cdrom);
			break;
		case 0x1B:
			cmd_reads(cdrom);
			break;
		case 0x1E:
			cmd_read_toc(cdrom);
			break;
		default:
			LOG_W("Unknown CD-ROM command (%02x)!\n",
				cdrom->cmd.code);
			break;
		}

	/* The busy flag is reset once the first response has been pushed to the
	FIFO and if previous interrupt has already been acknowledged. */
	if ((cdrom->cmd.step == 1) && (cdrom->int_flag.resp_received == INT0))
		cdrom->index_status.transmission_busy = 0;

	/* Consume cycles based on executed command */
	cmd_handle_timings(cdrom);

	/* Increment command step */
	cdrom->cmd.step++;
}

void psx_cdrom_cmd_tick(struct psx_cdrom *cdrom)
{
	int int_index;

	/* Transfer command to main loop if required */
	if (cdrom->cmd.complete && (cdrom->cmd.pending != 0)) {
		cdrom->cmd.code = cdrom->cmd.pending;
		cdrom->cmd.resp = INT0;
		cdrom->cmd.step = 0;
		cdrom->cmd.complete = false;
		cdrom->cmd.pending = 0;
	}

	/* Disable clock and exit if command is complete or blocked */
	if (cdrom->cmd.complete || (cdrom->int_flag.resp_received != INT0)) {
		cdrom->cmd_clock.enabled = false;
		return;
	}

	/* Execute current command */
	psx_cdrom_execute_cmd(cdrom);

	/* Check if a command response needs to be sent */
	if (cdrom->cmd.resp != INT0) {
		/* Transfer response to register */
		cdrom->int_flag.resp_received = cdrom->cmd.resp;
		cdrom->cmd.resp = INT0;

		/* Interrupt CPU if interrupt is enabled */
		int_index = cdrom->int_flag.resp_received - 1;
		if (cdrom->int_enable.bits & (1 << int_index))
			cpu_interrupt(cdrom->irq);
	}
}

bool psx_cdrom_init(struct controller_instance *instance)
{
	struct psx_cdrom *cdrom;
	struct resource *res;
	char *source;
	bool result;

	/* Allocate private structure */
	instance->priv_data = calloc(1, sizeof(struct psx_cdrom));
	cdrom = instance->priv_data;

	/* Set CD-ROM source (either physical CD-ROM or file) */
	source = env_get_data_path();

	/* Initialize CD-ROM frontend and set drive status accordingly */
	result = cdrom_init(source);
	cdrom->drive_status = result ? DRIVE_LICENSED_MODE2 : DRIVE_NO_DISC;

	/* Set up memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	cdrom->region.area = res;
	cdrom->region.mops = &psx_cdrom_mops;
	cdrom->region.data = cdrom;
	memory_region_add(&cdrom->region);

	/* Add DMA channel */
	res = resource_get("dma",
		RESOURCE_DMA,
		instance->resources,
		instance->num_resources);
	cdrom->dma_channel.res = res;
	cdrom->dma_channel.ops = &psx_cdrom_dma_ops;
	cdrom->dma_channel.data = cdrom;
	dma_channel_add(&cdrom->dma_channel);

	/* Add command clock */
	res = resource_get("clk",
		RESOURCE_CLK,
		instance->resources,
		instance->num_resources);
	cdrom->cmd_clock.rate = res->data.clk;
	cdrom->cmd_clock.data = cdrom;
	cdrom->cmd_clock.tick = (clock_tick_t)psx_cdrom_cmd_tick;
	clock_add(&cdrom->cmd_clock);

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

	/* Make sure clocks are disabled */
	cdrom->cmd_clock.enabled = false;
}

void psx_cdrom_deinit(struct controller_instance *instance)
{
	struct psx_cdrom *cdrom = instance->priv_data;
	cdrom_deinit();
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

