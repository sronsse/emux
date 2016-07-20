#include <string.h>
#include <bitops.h>
#include <clock.h>
#include <controller.h>
#include <memory.h>

#define VOICE_VOLUME_LEFT(n)			(0x0000 + (n * 0x10))
#define VOICE_VOLUME_RIGHT(n)			(0x0002 + (n * 0x10))
#define VOICE_ADPCM_SAMPLE_RATE(n)		(0x0004 + (n * 0x10))
#define VOICE_ADPCM_START_ADDR(n)		(0x0006 + (n * 0x10))
#define VOICE_ADSR_LO(n)			(0x0008 + (n * 0x10))
#define VOICE_ADSR_HI(n)			(0x000A + (n * 0x10))
#define VOICE_ADSR_CURRENT_VOLUME(n)		(0x000C + (n * 0x10))
#define MAIN_VOLUME_LEFT			0x0180
#define MAIN_VOLUME_RIGHT			0x0182
#define REVERB_OUTPUT_VOLUME_LEFT		0x0184
#define REVERB_OUTPUT_VOLUME_RIGHT		0x0186
#define VOICE_KEY_ON_LO				0x0188
#define VOICE_KEY_ON_HI				0x018A
#define VOICE_KEY_OFF_LO			0x018C
#define VOICE_KEY_OFF_HI			0x018E
#define VOICE_PITCH_MODE_ENABLE_LO		0x0190
#define VOICE_PITCH_MODE_ENABLE_HI		0x0192
#define VOICE_NOISE_MODE_ENABLE_LO		0x0194
#define VOICE_NOISE_MODE_ENABLE_HI		0x0196
#define VOICE_REVERB_MODE_ENABLE_LO		0x0198
#define VOICE_REVERB_MODE_ENABLE_HI		0x019A
#define SND_RAM_REVERB_WORK_AREA_START_ADDR	0x01A2
#define SND_RAM_DATA_TRANSFER_ADDR		0x01A6
#define SND_RAM_DATA_TRANSFER_FIFO		0x01A8
#define SPUCNT					0x01AA
#define SND_RAM_DATA_TRANSFER_CTRL		0x01AC
#define SPUSTAT					0x01AE
#define CD_AUDIO_INPUT_VOL_LEFT			0x01B0
#define CD_AUDIO_INPUT_VOL_RIGHT		0x01B2
#define EXT_AUDIO_INPUT_VOL_LEFT		0x01B4
#define EXT_AUDIO_INPUT_VOL_RIGHT		0x01B6

#define SPU_RAM_SIZE				KB(512)
#define FIFO_SIZE				32

#define NUM_VOICES				24
#define VOICE_VOLUME_LEFT_INDEX(a)		((a - 0x0000) / 0x10)
#define VOICE_VOLUME_RIGHT_INDEX(a)		((a - 0x0002) / 0x10)
#define VOICE_ADPCM_SAMPLE_RATE_INDEX(a)	((a - 0x0004) / 0x10)
#define VOICE_ADPCM_START_ADDR_INDEX(a)		((a - 0x0006) / 0x10)
#define VOICE_ADSR_LO_INDEX(a)			((a - 0x0008) / 0x10)
#define VOICE_ADSR_HI_INDEX(a)			((a - 0x000A) / 0x10)
#define VOICE_ADSR_CURRENT_VOLUME_INDEX(a)	((a - 0x000C) / 0x10)

#define TRANSFER_NORMAL				2
#define TRANSFER_REP2				3
#define TRANSFER_REP4				4
#define TRANSFER_REP8				5

#define TRANSFER_MODE_STOP			0
#define TRANSFER_MODE_MANUAL_WRITE		1
#define TRANSFER_MODE_DMA_WRITE			2
#define TRANSFER_MODE_DMA_READ			3

union volume {
	uint16_t raw;
	struct {
		uint16_t mode:1;
		uint16_t unused:15;
	};
	struct {
		uint16_t unused2:1;
		uint16_t volume:15;
	} fixed;
	struct {
		uint16_t unused3:1;
		uint16_t mode:1;
		uint16_t dir:1;
		uint16_t phase:1;
		uint16_t unused4:5;
		uint16_t shift:5;
		uint16_t step:2;
	} sweep;
};

union voice_adsr {
	uint32_t raw;
	struct {
		uint16_t low;
		uint16_t high;
	};
	struct {
		uint32_t sustain_level:4;
		uint32_t decay_shift:4;
		uint32_t attack_step:2;
		uint32_t attack_shift:5;
		uint32_t attack_mode:1;
		uint32_t release_shift:5;
		uint32_t release_mode:1;
		uint32_t sustain_step:2;
		uint32_t sustain_shift:5;
		uint32_t unused:1;
		uint32_t sustain_dir:1;
		uint32_t sustain_mode:1;
	};
};

union voice_pitch_mode_en {
	uint32_t raw;
	struct {
		uint16_t low;
		uint16_t high;
	};
	struct {
		uint32_t unknown:1;
		uint32_t flags:23;
		uint32_t unused:8;
	};
};

union voice_noise_mode_en {
	uint32_t raw;
	struct {
		uint16_t low;
		uint16_t high;
	};
	struct {
		uint32_t noise:24;
		uint32_t unused:8;
	};
};

union voice_reverb_mode_en {
	uint32_t raw;
	struct {
		uint16_t low;
		uint16_t high;
	};
	struct {
		uint32_t destination:24;
		uint32_t unused:8;
	};
};

union cnt {
	uint16_t raw;
	struct {
		uint16_t cd_audio_en:1;
		uint16_t ext_audio_en:1;
		uint16_t cd_audio_reverb:1;
		uint16_t ext_audio_reverb:1;
		uint16_t snd_ram_tfer_mode:2;
		uint16_t irq_en:1;
		uint16_t reverb_master_en:1;
		uint16_t noise_freq_step:2;
		uint16_t noise_freq_shift:4;
		uint16_t mute:1;
		uint16_t enable:1;
	};
};

union snd_ram_data_transfer_ctrl {
	uint16_t raw;
	struct {
		uint16_t unknown:1;
		uint16_t transfer_type:3;
		uint16_t unknown2:12;
	};
};

union stat {
	uint16_t raw;
	struct {
		uint16_t cd_audio_en:1;
		uint16_t ext_audio_en:1;
		uint16_t cd_audio_reverb:1;
		uint16_t ext_audio_reverb:1;
		uint16_t snd_ram_tfer_mode:2;
		uint16_t irq:1;
		uint16_t data_transfer_dma_rw_req:1;
		uint16_t data_transfer_dma_r_req:1;
		uint16_t data_transfer_dma_w_req:1;
		uint16_t data_transfer_busy:1;
		uint16_t writing_to_second_half:1;
		uint16_t unknown:4;
	};
};

struct fifo {
	uint16_t data[FIFO_SIZE];
	int pos;
	int num;
};

struct spu {
	union volume voice_vol_left[NUM_VOICES];
	union volume voice_vol_right[NUM_VOICES];
	uint16_t voice_adpcm_sample_rate[NUM_VOICES];
	uint16_t voice_adpcm_start_addr[NUM_VOICES];
	union voice_adsr voice_adsr[NUM_VOICES];
	int16_t voice_adsr_current_vol[NUM_VOICES];
	union volume main_vol_left;
	union volume main_vol_right;
	int16_t reverb_output_vol_left;
	int16_t reverb_output_vol_right;
	union voice_pitch_mode_en voice_pitch_mode_en;
	union voice_noise_mode_en voice_noise_mode_en;
	union voice_reverb_mode_en voice_reverb_mode_en;
	uint16_t snd_ram_reverb_work_area_start_addr;
	uint16_t snd_ram_data_addr;
	union cnt cnt;
	union snd_ram_data_transfer_ctrl snd_ram_data_transfer_ctrl;
	union stat stat;
	int16_t cd_audio_input_vol_left;
	int16_t cd_audio_input_vol_right;
	int16_t ext_audio_input_vol_left;
	int16_t ext_audio_input_vol_right;
	uint32_t cur_snd_ram_data_addr;
	uint8_t ram[SPU_RAM_SIZE];
	struct fifo fifo;
	struct region region;
	struct dma_channel dma_channel;
};

static bool spu_init(struct controller_instance *instance);
static void spu_reset(struct controller_instance *instance);
static void spu_deinit(struct controller_instance *instance);
static uint16_t spu_readw(struct spu *spu, address_t address);
static void spu_writew(struct spu *spu, uint16_t w, address_t address);
static uint32_t spu_dma_readl(struct spu *spu);
static void spu_dma_writel(struct spu *spu, uint32_t l);
static void spu_writew(struct spu *spu, uint16_t w, address_t address);
static void spu_fifo_enqueue(struct spu *spu, uint16_t data);
static bool spu_fifo_dequeue(struct spu *spu, uint16_t *data, int size);

static struct mops spu_mops = {
	.readw = (readw_t)spu_readw,
	.writew = (writew_t)spu_writew
};

static struct dma_ops spu_dma_ops = {
	.readl = (dma_readl_t)spu_dma_readl,
	.writel = (dma_writel_t)spu_dma_writel
};

uint16_t spu_readw(struct spu *spu, address_t address)
{
	uint16_t w;
	int voice;

	/* Handle register read */
	switch (address) {
	case VOICE_VOLUME_LEFT(0):
	case VOICE_VOLUME_LEFT(1):
	case VOICE_VOLUME_LEFT(2):
	case VOICE_VOLUME_LEFT(3):
	case VOICE_VOLUME_LEFT(4):
	case VOICE_VOLUME_LEFT(5):
	case VOICE_VOLUME_LEFT(6):
	case VOICE_VOLUME_LEFT(7):
	case VOICE_VOLUME_LEFT(8):
	case VOICE_VOLUME_LEFT(9):
	case VOICE_VOLUME_LEFT(10):
	case VOICE_VOLUME_LEFT(11):
	case VOICE_VOLUME_LEFT(12):
	case VOICE_VOLUME_LEFT(13):
	case VOICE_VOLUME_LEFT(14):
	case VOICE_VOLUME_LEFT(15):
	case VOICE_VOLUME_LEFT(16):
	case VOICE_VOLUME_LEFT(17):
	case VOICE_VOLUME_LEFT(18):
	case VOICE_VOLUME_LEFT(19):
	case VOICE_VOLUME_LEFT(20):
	case VOICE_VOLUME_LEFT(21):
	case VOICE_VOLUME_LEFT(22):
	case VOICE_VOLUME_LEFT(23):
		/* Get voice from address and read its volume left register */
		voice = VOICE_VOLUME_LEFT_INDEX(address);
		w = spu->voice_vol_left[voice].raw;
		break;
	case VOICE_VOLUME_RIGHT(0):
	case VOICE_VOLUME_RIGHT(1):
	case VOICE_VOLUME_RIGHT(2):
	case VOICE_VOLUME_RIGHT(3):
	case VOICE_VOLUME_RIGHT(4):
	case VOICE_VOLUME_RIGHT(5):
	case VOICE_VOLUME_RIGHT(6):
	case VOICE_VOLUME_RIGHT(7):
	case VOICE_VOLUME_RIGHT(8):
	case VOICE_VOLUME_RIGHT(9):
	case VOICE_VOLUME_RIGHT(10):
	case VOICE_VOLUME_RIGHT(11):
	case VOICE_VOLUME_RIGHT(12):
	case VOICE_VOLUME_RIGHT(13):
	case VOICE_VOLUME_RIGHT(14):
	case VOICE_VOLUME_RIGHT(15):
	case VOICE_VOLUME_RIGHT(16):
	case VOICE_VOLUME_RIGHT(17):
	case VOICE_VOLUME_RIGHT(18):
	case VOICE_VOLUME_RIGHT(19):
	case VOICE_VOLUME_RIGHT(20):
	case VOICE_VOLUME_RIGHT(21):
	case VOICE_VOLUME_RIGHT(22):
	case VOICE_VOLUME_RIGHT(23):
		/* Get voice from address and read its volume right register */
		voice = VOICE_VOLUME_RIGHT_INDEX(address);
		w = spu->voice_vol_right[voice].raw;
		break;
	case VOICE_ADPCM_SAMPLE_RATE(0):
	case VOICE_ADPCM_SAMPLE_RATE(1):
	case VOICE_ADPCM_SAMPLE_RATE(2):
	case VOICE_ADPCM_SAMPLE_RATE(3):
	case VOICE_ADPCM_SAMPLE_RATE(4):
	case VOICE_ADPCM_SAMPLE_RATE(5):
	case VOICE_ADPCM_SAMPLE_RATE(6):
	case VOICE_ADPCM_SAMPLE_RATE(7):
	case VOICE_ADPCM_SAMPLE_RATE(8):
	case VOICE_ADPCM_SAMPLE_RATE(9):
	case VOICE_ADPCM_SAMPLE_RATE(10):
	case VOICE_ADPCM_SAMPLE_RATE(11):
	case VOICE_ADPCM_SAMPLE_RATE(12):
	case VOICE_ADPCM_SAMPLE_RATE(13):
	case VOICE_ADPCM_SAMPLE_RATE(14):
	case VOICE_ADPCM_SAMPLE_RATE(15):
	case VOICE_ADPCM_SAMPLE_RATE(16):
	case VOICE_ADPCM_SAMPLE_RATE(17):
	case VOICE_ADPCM_SAMPLE_RATE(18):
	case VOICE_ADPCM_SAMPLE_RATE(19):
	case VOICE_ADPCM_SAMPLE_RATE(20):
	case VOICE_ADPCM_SAMPLE_RATE(21):
	case VOICE_ADPCM_SAMPLE_RATE(22):
	case VOICE_ADPCM_SAMPLE_RATE(23):
		/* Get voice from address and read its ADPCM sample rate */
		voice = VOICE_ADPCM_SAMPLE_RATE_INDEX(address);
		w = spu->voice_adpcm_sample_rate[voice];
		break;
	case VOICE_ADPCM_START_ADDR(0):
	case VOICE_ADPCM_START_ADDR(1):
	case VOICE_ADPCM_START_ADDR(2):
	case VOICE_ADPCM_START_ADDR(3):
	case VOICE_ADPCM_START_ADDR(4):
	case VOICE_ADPCM_START_ADDR(5):
	case VOICE_ADPCM_START_ADDR(6):
	case VOICE_ADPCM_START_ADDR(7):
	case VOICE_ADPCM_START_ADDR(8):
	case VOICE_ADPCM_START_ADDR(9):
	case VOICE_ADPCM_START_ADDR(10):
	case VOICE_ADPCM_START_ADDR(11):
	case VOICE_ADPCM_START_ADDR(12):
	case VOICE_ADPCM_START_ADDR(13):
	case VOICE_ADPCM_START_ADDR(14):
	case VOICE_ADPCM_START_ADDR(15):
	case VOICE_ADPCM_START_ADDR(16):
	case VOICE_ADPCM_START_ADDR(17):
	case VOICE_ADPCM_START_ADDR(18):
	case VOICE_ADPCM_START_ADDR(19):
	case VOICE_ADPCM_START_ADDR(20):
	case VOICE_ADPCM_START_ADDR(21):
	case VOICE_ADPCM_START_ADDR(22):
	case VOICE_ADPCM_START_ADDR(23):
		/* Get voice from address and read its ADPCM start address */
		voice = VOICE_ADPCM_START_ADDR_INDEX(address);
		w = spu->voice_adpcm_start_addr[voice];
		break;
	case VOICE_ADSR_LO(0):
	case VOICE_ADSR_LO(1):
	case VOICE_ADSR_LO(2):
	case VOICE_ADSR_LO(3):
	case VOICE_ADSR_LO(4):
	case VOICE_ADSR_LO(5):
	case VOICE_ADSR_LO(6):
	case VOICE_ADSR_LO(7):
	case VOICE_ADSR_LO(8):
	case VOICE_ADSR_LO(9):
	case VOICE_ADSR_LO(10):
	case VOICE_ADSR_LO(11):
	case VOICE_ADSR_LO(12):
	case VOICE_ADSR_LO(13):
	case VOICE_ADSR_LO(14):
	case VOICE_ADSR_LO(15):
	case VOICE_ADSR_LO(16):
	case VOICE_ADSR_LO(17):
	case VOICE_ADSR_LO(18):
	case VOICE_ADSR_LO(19):
	case VOICE_ADSR_LO(20):
	case VOICE_ADSR_LO(21):
	case VOICE_ADSR_LO(22):
	case VOICE_ADSR_LO(23):
		/* Get voice from address and read its ADSR low register */
		voice = VOICE_ADSR_LO_INDEX(address);
		w = spu->voice_adsr[voice].low;
		break;
	case VOICE_ADSR_HI(0):
	case VOICE_ADSR_HI(1):
	case VOICE_ADSR_HI(2):
	case VOICE_ADSR_HI(3):
	case VOICE_ADSR_HI(4):
	case VOICE_ADSR_HI(5):
	case VOICE_ADSR_HI(6):
	case VOICE_ADSR_HI(7):
	case VOICE_ADSR_HI(8):
	case VOICE_ADSR_HI(9):
	case VOICE_ADSR_HI(10):
	case VOICE_ADSR_HI(11):
	case VOICE_ADSR_HI(12):
	case VOICE_ADSR_HI(13):
	case VOICE_ADSR_HI(14):
	case VOICE_ADSR_HI(15):
	case VOICE_ADSR_HI(16):
	case VOICE_ADSR_HI(17):
	case VOICE_ADSR_HI(18):
	case VOICE_ADSR_HI(19):
	case VOICE_ADSR_HI(20):
	case VOICE_ADSR_HI(21):
	case VOICE_ADSR_HI(22):
	case VOICE_ADSR_HI(23):
		/* Get voice from address and read its ADSR high register */
		voice = VOICE_ADSR_HI_INDEX(address);
		w = spu->voice_adsr[voice].high;
		break;
	case VOICE_ADSR_CURRENT_VOLUME(0):
	case VOICE_ADSR_CURRENT_VOLUME(1):
	case VOICE_ADSR_CURRENT_VOLUME(2):
	case VOICE_ADSR_CURRENT_VOLUME(3):
	case VOICE_ADSR_CURRENT_VOLUME(4):
	case VOICE_ADSR_CURRENT_VOLUME(5):
	case VOICE_ADSR_CURRENT_VOLUME(6):
	case VOICE_ADSR_CURRENT_VOLUME(7):
	case VOICE_ADSR_CURRENT_VOLUME(8):
	case VOICE_ADSR_CURRENT_VOLUME(9):
	case VOICE_ADSR_CURRENT_VOLUME(10):
	case VOICE_ADSR_CURRENT_VOLUME(11):
	case VOICE_ADSR_CURRENT_VOLUME(12):
	case VOICE_ADSR_CURRENT_VOLUME(13):
	case VOICE_ADSR_CURRENT_VOLUME(14):
	case VOICE_ADSR_CURRENT_VOLUME(15):
	case VOICE_ADSR_CURRENT_VOLUME(16):
	case VOICE_ADSR_CURRENT_VOLUME(17):
	case VOICE_ADSR_CURRENT_VOLUME(18):
	case VOICE_ADSR_CURRENT_VOLUME(19):
	case VOICE_ADSR_CURRENT_VOLUME(20):
	case VOICE_ADSR_CURRENT_VOLUME(21):
	case VOICE_ADSR_CURRENT_VOLUME(22):
	case VOICE_ADSR_CURRENT_VOLUME(23):
		/* Get voice from address and read its current ADSR volume */
		voice = VOICE_ADSR_CURRENT_VOLUME_INDEX(address);
		w = spu->voice_adsr_current_vol[voice];
		break;
	case MAIN_VOLUME_LEFT:
		/* Read main volume left register */
		w = spu->main_vol_left.raw;
		break;
	case MAIN_VOLUME_RIGHT:
		/* Read main volume right register */
		w = spu->main_vol_right.raw;
		break;
	case REVERB_OUTPUT_VOLUME_LEFT:
		/* Read reverb output volume left register */
		w = spu->reverb_output_vol_left;
		break;
	case REVERB_OUTPUT_VOLUME_RIGHT:
		/* Read reverb output volume right register */
		w = spu->reverb_output_vol_right;
		break;
	case VOICE_PITCH_MODE_ENABLE_LO:
		/* Read voice pitch mode enable register (low) */
		w = spu->voice_pitch_mode_en.low;
		break;
	case VOICE_PITCH_MODE_ENABLE_HI:
		/* Read voice pitch mode enable register (high) */
		w = spu->voice_pitch_mode_en.high;
		break;
	case VOICE_NOISE_MODE_ENABLE_LO:
		/* Read voice noise mode enable register (low) */
		w = spu->voice_noise_mode_en.low;
		break;
	case VOICE_NOISE_MODE_ENABLE_HI:
		/* Read voice noise mode enable register (high) */
		w = spu->voice_noise_mode_en.high;
		break;
	case VOICE_REVERB_MODE_ENABLE_LO:
		/* Read voice reverb mode enable register (low) */
		w = spu->voice_reverb_mode_en.low;
		break;
	case VOICE_REVERB_MODE_ENABLE_HI:
		/* Read voice reverb mode enable register (high) */
		w = spu->voice_reverb_mode_en.high;
		break;
	case SND_RAM_REVERB_WORK_AREA_START_ADDR:
		/* Read sound RAM reverb work area start address register */
		w = spu->snd_ram_reverb_work_area_start_addr;
		break;
	case SND_RAM_DATA_TRANSFER_ADDR:
		/* Read sound RAM data transfer address register */
		w = spu->snd_ram_data_addr;
		break;
	case SPUCNT:
		/* Read control register */
		w = spu->cnt.raw;
		break;
	case SND_RAM_DATA_TRANSFER_CTRL:
		/* Read sound RAM data transfer control register */
		w = spu->snd_ram_data_transfer_ctrl.raw;
		break;
	case SPUSTAT:
		/* Read status register */
		w = spu->stat.raw;
		break;
	case CD_AUDIO_INPUT_VOL_LEFT:
		/* Read CD audio input volume left register */
		w = spu->cd_audio_input_vol_left;
		break;
	case CD_AUDIO_INPUT_VOL_RIGHT:
		/* Read CD audio input volume right register */
		w = spu->cd_audio_input_vol_right;
		break;
	case EXT_AUDIO_INPUT_VOL_LEFT:
		/* Read external audio input volume left register */
		w = spu->ext_audio_input_vol_left;
		break;
	case EXT_AUDIO_INPUT_VOL_RIGHT:
		/* Read external audio input volume right register */
		w = spu->ext_audio_input_vol_right;
		break;
	default:
		LOG_D("Unknown read at %x\n", address);
		w = 0;
		break;
	}

	/* Return word */
	return w;
}

void spu_writew(struct spu *spu, uint16_t w, address_t address)
{
	uint32_t l = 0;
	bool rreq;
	bool wreq;
	int voice;
	int i;

	/* Handle register write */
	switch (address) {
	case VOICE_VOLUME_LEFT(0):
	case VOICE_VOLUME_LEFT(1):
	case VOICE_VOLUME_LEFT(2):
	case VOICE_VOLUME_LEFT(3):
	case VOICE_VOLUME_LEFT(4):
	case VOICE_VOLUME_LEFT(5):
	case VOICE_VOLUME_LEFT(6):
	case VOICE_VOLUME_LEFT(7):
	case VOICE_VOLUME_LEFT(8):
	case VOICE_VOLUME_LEFT(9):
	case VOICE_VOLUME_LEFT(10):
	case VOICE_VOLUME_LEFT(11):
	case VOICE_VOLUME_LEFT(12):
	case VOICE_VOLUME_LEFT(13):
	case VOICE_VOLUME_LEFT(14):
	case VOICE_VOLUME_LEFT(15):
	case VOICE_VOLUME_LEFT(16):
	case VOICE_VOLUME_LEFT(17):
	case VOICE_VOLUME_LEFT(18):
	case VOICE_VOLUME_LEFT(19):
	case VOICE_VOLUME_LEFT(20):
	case VOICE_VOLUME_LEFT(21):
	case VOICE_VOLUME_LEFT(22):
	case VOICE_VOLUME_LEFT(23):
		/* Get voice from address and write its volume left register */
		voice = VOICE_VOLUME_LEFT_INDEX(address);
		spu->voice_vol_left[voice].raw = w;
		break;
	case VOICE_VOLUME_RIGHT(0):
	case VOICE_VOLUME_RIGHT(1):
	case VOICE_VOLUME_RIGHT(2):
	case VOICE_VOLUME_RIGHT(3):
	case VOICE_VOLUME_RIGHT(4):
	case VOICE_VOLUME_RIGHT(5):
	case VOICE_VOLUME_RIGHT(6):
	case VOICE_VOLUME_RIGHT(7):
	case VOICE_VOLUME_RIGHT(8):
	case VOICE_VOLUME_RIGHT(9):
	case VOICE_VOLUME_RIGHT(10):
	case VOICE_VOLUME_RIGHT(11):
	case VOICE_VOLUME_RIGHT(12):
	case VOICE_VOLUME_RIGHT(13):
	case VOICE_VOLUME_RIGHT(14):
	case VOICE_VOLUME_RIGHT(15):
	case VOICE_VOLUME_RIGHT(16):
	case VOICE_VOLUME_RIGHT(17):
	case VOICE_VOLUME_RIGHT(18):
	case VOICE_VOLUME_RIGHT(19):
	case VOICE_VOLUME_RIGHT(20):
	case VOICE_VOLUME_RIGHT(21):
	case VOICE_VOLUME_RIGHT(22):
	case VOICE_VOLUME_RIGHT(23):
		/* Get voice from address and write its volume right register */
		voice = VOICE_VOLUME_RIGHT_INDEX(address);
		spu->voice_vol_right[voice].raw = w;
		break;
	case VOICE_ADPCM_SAMPLE_RATE(0):
	case VOICE_ADPCM_SAMPLE_RATE(1):
	case VOICE_ADPCM_SAMPLE_RATE(2):
	case VOICE_ADPCM_SAMPLE_RATE(3):
	case VOICE_ADPCM_SAMPLE_RATE(4):
	case VOICE_ADPCM_SAMPLE_RATE(5):
	case VOICE_ADPCM_SAMPLE_RATE(6):
	case VOICE_ADPCM_SAMPLE_RATE(7):
	case VOICE_ADPCM_SAMPLE_RATE(8):
	case VOICE_ADPCM_SAMPLE_RATE(9):
	case VOICE_ADPCM_SAMPLE_RATE(10):
	case VOICE_ADPCM_SAMPLE_RATE(11):
	case VOICE_ADPCM_SAMPLE_RATE(12):
	case VOICE_ADPCM_SAMPLE_RATE(13):
	case VOICE_ADPCM_SAMPLE_RATE(14):
	case VOICE_ADPCM_SAMPLE_RATE(15):
	case VOICE_ADPCM_SAMPLE_RATE(16):
	case VOICE_ADPCM_SAMPLE_RATE(17):
	case VOICE_ADPCM_SAMPLE_RATE(18):
	case VOICE_ADPCM_SAMPLE_RATE(19):
	case VOICE_ADPCM_SAMPLE_RATE(20):
	case VOICE_ADPCM_SAMPLE_RATE(21):
	case VOICE_ADPCM_SAMPLE_RATE(22):
	case VOICE_ADPCM_SAMPLE_RATE(23):
		/* Get voice from address and write its ADPCM sample rate */
		voice = VOICE_ADPCM_SAMPLE_RATE_INDEX(address);
		spu->voice_adpcm_sample_rate[voice] = w;
		break;
	case VOICE_ADPCM_START_ADDR(0):
	case VOICE_ADPCM_START_ADDR(1):
	case VOICE_ADPCM_START_ADDR(2):
	case VOICE_ADPCM_START_ADDR(3):
	case VOICE_ADPCM_START_ADDR(4):
	case VOICE_ADPCM_START_ADDR(5):
	case VOICE_ADPCM_START_ADDR(6):
	case VOICE_ADPCM_START_ADDR(7):
	case VOICE_ADPCM_START_ADDR(8):
	case VOICE_ADPCM_START_ADDR(9):
	case VOICE_ADPCM_START_ADDR(10):
	case VOICE_ADPCM_START_ADDR(11):
	case VOICE_ADPCM_START_ADDR(12):
	case VOICE_ADPCM_START_ADDR(13):
	case VOICE_ADPCM_START_ADDR(14):
	case VOICE_ADPCM_START_ADDR(15):
	case VOICE_ADPCM_START_ADDR(16):
	case VOICE_ADPCM_START_ADDR(17):
	case VOICE_ADPCM_START_ADDR(18):
	case VOICE_ADPCM_START_ADDR(19):
	case VOICE_ADPCM_START_ADDR(20):
	case VOICE_ADPCM_START_ADDR(21):
	case VOICE_ADPCM_START_ADDR(22):
	case VOICE_ADPCM_START_ADDR(23):
		/* Get voice from address and write its ADPCM start address */
		voice = VOICE_ADPCM_START_ADDR_INDEX(address);
		spu->voice_adpcm_start_addr[voice] = w;
		break;
	case VOICE_ADSR_LO(0):
	case VOICE_ADSR_LO(1):
	case VOICE_ADSR_LO(2):
	case VOICE_ADSR_LO(3):
	case VOICE_ADSR_LO(4):
	case VOICE_ADSR_LO(5):
	case VOICE_ADSR_LO(6):
	case VOICE_ADSR_LO(7):
	case VOICE_ADSR_LO(8):
	case VOICE_ADSR_LO(9):
	case VOICE_ADSR_LO(10):
	case VOICE_ADSR_LO(11):
	case VOICE_ADSR_LO(12):
	case VOICE_ADSR_LO(13):
	case VOICE_ADSR_LO(14):
	case VOICE_ADSR_LO(15):
	case VOICE_ADSR_LO(16):
	case VOICE_ADSR_LO(17):
	case VOICE_ADSR_LO(18):
	case VOICE_ADSR_LO(19):
	case VOICE_ADSR_LO(20):
	case VOICE_ADSR_LO(21):
	case VOICE_ADSR_LO(22):
	case VOICE_ADSR_LO(23):
		/* Get voice from address and write its ADSR low register */
		voice = VOICE_ADSR_LO_INDEX(address);
		spu->voice_adsr[voice].low = w;
		break;
	case VOICE_ADSR_HI(0):
	case VOICE_ADSR_HI(1):
	case VOICE_ADSR_HI(2):
	case VOICE_ADSR_HI(3):
	case VOICE_ADSR_HI(4):
	case VOICE_ADSR_HI(5):
	case VOICE_ADSR_HI(6):
	case VOICE_ADSR_HI(7):
	case VOICE_ADSR_HI(8):
	case VOICE_ADSR_HI(9):
	case VOICE_ADSR_HI(10):
	case VOICE_ADSR_HI(11):
	case VOICE_ADSR_HI(12):
	case VOICE_ADSR_HI(13):
	case VOICE_ADSR_HI(14):
	case VOICE_ADSR_HI(15):
	case VOICE_ADSR_HI(16):
	case VOICE_ADSR_HI(17):
	case VOICE_ADSR_HI(18):
	case VOICE_ADSR_HI(19):
	case VOICE_ADSR_HI(20):
	case VOICE_ADSR_HI(21):
	case VOICE_ADSR_HI(22):
	case VOICE_ADSR_HI(23):
		/* Get voice from address and write its ADSR high register */
		voice = VOICE_ADSR_HI_INDEX(address);
		spu->voice_adsr[voice].high = w;
		break;
	case VOICE_ADSR_CURRENT_VOLUME(0):
	case VOICE_ADSR_CURRENT_VOLUME(1):
	case VOICE_ADSR_CURRENT_VOLUME(2):
	case VOICE_ADSR_CURRENT_VOLUME(3):
	case VOICE_ADSR_CURRENT_VOLUME(4):
	case VOICE_ADSR_CURRENT_VOLUME(5):
	case VOICE_ADSR_CURRENT_VOLUME(6):
	case VOICE_ADSR_CURRENT_VOLUME(7):
	case VOICE_ADSR_CURRENT_VOLUME(8):
	case VOICE_ADSR_CURRENT_VOLUME(9):
	case VOICE_ADSR_CURRENT_VOLUME(10):
	case VOICE_ADSR_CURRENT_VOLUME(11):
	case VOICE_ADSR_CURRENT_VOLUME(12):
	case VOICE_ADSR_CURRENT_VOLUME(13):
	case VOICE_ADSR_CURRENT_VOLUME(14):
	case VOICE_ADSR_CURRENT_VOLUME(15):
	case VOICE_ADSR_CURRENT_VOLUME(16):
	case VOICE_ADSR_CURRENT_VOLUME(17):
	case VOICE_ADSR_CURRENT_VOLUME(18):
	case VOICE_ADSR_CURRENT_VOLUME(19):
	case VOICE_ADSR_CURRENT_VOLUME(20):
	case VOICE_ADSR_CURRENT_VOLUME(21):
	case VOICE_ADSR_CURRENT_VOLUME(22):
	case VOICE_ADSR_CURRENT_VOLUME(23):
		/* Get voice from address and write its current ADSR volume */
		voice = VOICE_ADSR_CURRENT_VOLUME_INDEX(address);
		spu->voice_adsr_current_vol[voice] = w;
		break;
	case MAIN_VOLUME_LEFT:
		/* Write main volume left register */
		spu->main_vol_left.raw = w;
		break;
	case MAIN_VOLUME_RIGHT:
		/* Write main volume right register */
		spu->main_vol_right.raw = w;
		break;
	case REVERB_OUTPUT_VOLUME_LEFT:
		/* Write reverb output volume left register */
		spu->reverb_output_vol_left = w;
		break;
	case REVERB_OUTPUT_VOLUME_RIGHT:
		/* Write reverb output volume right register */
		spu->reverb_output_vol_right = w;
		break;
	case VOICE_KEY_ON_LO:
	case VOICE_KEY_ON_HI:
		/* Recompose value appropriately based on address */
		bitops_setl(&l,
			(address == VOICE_KEY_ON_LO) ? 0 : 16,
			16,
			w);

		/* Handle each voice key on event */
		for (i = 0; i < NUM_VOICES; i++)
			if (l & (1 << i))
				LOG_D("Voice %u key on\n", i);
		break;
	case VOICE_KEY_OFF_LO:
	case VOICE_KEY_OFF_HI:
		/* Recompose value appropriately based on address */
		bitops_setl(&l,
			(address == VOICE_KEY_OFF_LO) ? 0 : 16,
			16,
			w);

		/* Handle each voice key off event */
		for (i = 0; i < NUM_VOICES; i++)
			if (l & (1 << i))
				LOG_D("Voice %u key off\n", i);
		break;
	case VOICE_PITCH_MODE_ENABLE_LO:
		/* Write voice pitch mode enable register (low) */
		spu->voice_pitch_mode_en.low = w;
		break;
	case VOICE_PITCH_MODE_ENABLE_HI:
		/* Write voice pitch mode enable register (high) */
		spu->voice_pitch_mode_en.high = w;
		break;
	case VOICE_NOISE_MODE_ENABLE_LO:
		/* Write voice noise mode enable register (low) */
		spu->voice_noise_mode_en.low = w;
		break;
	case VOICE_NOISE_MODE_ENABLE_HI:
		/* Write voice noise mode enable register (high) */
		spu->voice_noise_mode_en.high = w;
		break;
	case VOICE_REVERB_MODE_ENABLE_LO:
		/* Write voice reverb mode enable register (low) */
		spu->voice_reverb_mode_en.low = w;
		break;
	case VOICE_REVERB_MODE_ENABLE_HI:
		/* Write voice reverb mode enable register (high) */
		spu->voice_reverb_mode_en.high = w;
		break;
	case SND_RAM_REVERB_WORK_AREA_START_ADDR:
		/* Write sound RAM reverb work area start address register */
		spu->snd_ram_reverb_work_area_start_addr = w;
		break;
	case SND_RAM_DATA_TRANSFER_ADDR:
		/* Write sound RAM data transfer address register */
		spu->snd_ram_data_addr = w;

		/* Writing to this register additionally stores the value
		(multiplied by 8) in the internal "current address" register. */
		spu->cur_snd_ram_data_addr = w * 8;
		break;
	case SND_RAM_DATA_TRANSFER_FIFO:
		/* Enqueue data into FIFO (if transfer mode is set properly) */
		if (spu->stat.snd_ram_tfer_mode == TRANSFER_MODE_MANUAL_WRITE)
			spu_fifo_enqueue(spu, w);
		break;
	case SPUCNT:
		/* Write control register */
		spu->cnt.raw = w;

		/* Apply bits 0-5 to SPUSTAT */
		spu->stat.cd_audio_en = spu->cnt.cd_audio_en;
		spu->stat.ext_audio_en = spu->cnt.ext_audio_en;
		spu->stat.cd_audio_reverb = spu->cnt.cd_audio_reverb;
		spu->stat.ext_audio_reverb = spu->cnt.ext_audio_reverb;
		spu->stat.snd_ram_tfer_mode = spu->cnt.snd_ram_tfer_mode;

		/* Update DMA transfer write request bit in SPUSTAT */
		wreq = (spu->stat.snd_ram_tfer_mode == TRANSFER_MODE_DMA_WRITE);
		spu->stat.data_transfer_dma_w_req = wreq;

		/* Update DMA transfer read request bit in SPUSTAT */
		rreq = (spu->stat.snd_ram_tfer_mode == TRANSFER_MODE_DMA_READ);
		spu->stat.data_transfer_dma_r_req = rreq;

		/* Update DMA transfer read/write request bit in SPUSTAT */
		spu->stat.data_transfer_dma_rw_req = wreq | rreq;
		break;
	case SND_RAM_DATA_TRANSFER_CTRL:
		/* Write sound RAM data transfer control register */
		spu->snd_ram_data_transfer_ctrl.raw = w;
		break;
	case CD_AUDIO_INPUT_VOL_LEFT:
		/* Write CD audio input volume left register */
		spu->cd_audio_input_vol_left = w;
		break;
	case CD_AUDIO_INPUT_VOL_RIGHT:
		/* Write CD audio input volume right register */
		spu->cd_audio_input_vol_right = w;
		break;
	case EXT_AUDIO_INPUT_VOL_LEFT:
		/* Write external audio input volume left register */
		spu->ext_audio_input_vol_left = w;
		break;
	case EXT_AUDIO_INPUT_VOL_RIGHT:
		/* Write external audio input volume right register */
		spu->ext_audio_input_vol_right = w;
		break;
	default:
		LOG_D("Unknown write %x at %x\n", w, address);
		break;
	}
}

uint32_t spu_dma_readl(struct spu *spu)
{
	uint32_t l;

	/* Consume 4 clks/word */
	clock_consume(4);

	/* Return already if transfer mode is not properly set */
	if (spu->stat.snd_ram_tfer_mode != TRANSFER_MODE_DMA_READ)
		return 0;

	/* Read 4 bytes from SPU RAM and return combined value */
	l = spu->ram[spu->cur_snd_ram_data_addr++];
	l |= spu->ram[spu->cur_snd_ram_data_addr++] << 8;
	l |= spu->ram[spu->cur_snd_ram_data_addr++] << 16;
	l |= spu->ram[spu->cur_snd_ram_data_addr++] << 24;
	return l;
}

void spu_dma_writel(struct spu *spu, uint32_t l)
{
	uint16_t w;

	/* Consume 4 clks/word */
	clock_consume(4);

	/* Return already if transfer mode is not properly set */
	if (spu->stat.snd_ram_tfer_mode != TRANSFER_MODE_DMA_WRITE)
		return;

	/* Enqueue each half-word into SPU RAM FIFO */
	w = l & 0xFFFF;
	spu_fifo_enqueue(spu, w);
	w = l >> 16;
	spu_fifo_enqueue(spu, w);
}

void spu_fifo_enqueue(struct spu *spu, uint16_t data)
{
	uint16_t words[FIFO_SIZE];
	uint16_t w;
	int num;
	int index;
	int i;

	/* Add data to FIFO and handle position overflow */
	spu->fifo.data[spu->fifo.pos++] = data;
	if (spu->fifo.pos == FIFO_SIZE)
		spu->fifo.pos = 0;

	/* Increment number of elements */
	spu->fifo.num++;

	/* The transfer type selects how data is forwarded to SPU RAM:
	Transfer Type	Words in FIFO		Words written to SPU RAM
	0,1,6,7 (Fill)	A,B,C,D,E,F,G,H,...,X	X,X,X,X,X,X,X,X,...
	2 (Normal)	A,B,C,D,E,F,G,H,...,X	A,B,C,D,E,F,G,H,...
	3 (Rep2)	A,B,C,D,E,F,G,H,...,X	A,A,C,C,E,E,G,G,...
	4 (Rep4)	A,B,C,D,E,F,G,H,...,X	A,A,A,A,E,E,E,E,...
	5 (Rep8)	A,B,C,D,E,F,G,H,...,X	H,H,H,H,H,H,H,H,... */

	/* Handle FIFO to SPU RAM transfers */
	for (;;) {
		/* Handle SPU RAM transfer if possible */
		switch (spu->snd_ram_data_transfer_ctrl.transfer_type) {
		case TRANSFER_NORMAL:
			/* Normal transfer saves every word */
			num = 1;
			index = 0;
			break;
		case TRANSFER_REP2:
			/* Rep2 skips the 2nd word */
			num = 2;
			index = 0;
			break;
		case TRANSFER_REP4:
			/* Rep4 skips the 2nd..4th words */
			num = 4;
			index = 0;
			break;
		case TRANSFER_REP8:
			/* Rep8 skips the 1st..7th words */
			num = 8;
			index = 7;
			break;
		default:
			/* Fill uses only the last word in the FIFO */
			num = FIFO_SIZE;
			index = FIFO_SIZE - 1;
			break;
		}

		/* Dequeue FIFO, stopping if no further transfer is possible */
		if (!spu_fifo_dequeue(spu, words, num))
			break;

		/* Get word to transfer */
		w = words[index];

		/* Transfer words to SPU RAM, increasing transfer address */
		for (i = 0; i < num; i++) {
			spu->ram[spu->cur_snd_ram_data_addr++] = w & 0xFF;
			spu->ram[spu->cur_snd_ram_data_addr++] = w >> 8;
		}
	}
}

bool spu_fifo_dequeue(struct spu *spu, uint16_t *data, int size)
{
	int index;
	int i;

	/* Return if FIFO does not have enough elements */
	if (spu->fifo.num < size)
		return false;

	/* Remove data from FIFO */
	for (i = 0; i < size; i++) {
		index = (spu->fifo.pos - spu->fifo.num) + FIFO_SIZE;
		index %= FIFO_SIZE;
		data[i] = spu->fifo.data[index];
		spu->fifo.num--;
	}

	/* Return success */
	return true;
}

bool spu_init(struct controller_instance *instance)
{
	struct spu *spu;
	struct resource *res;

	/* Allocate SPU structure */
	instance->priv_data = calloc(1, sizeof(struct spu));
	spu = instance->priv_data;

	/* Add SPU memory region */
	res = resource_get("mem",
		RESOURCE_MEM,
		instance->resources,
		instance->num_resources);
	spu->region.area = res;
	spu->region.mops = &spu_mops;
	spu->region.data = spu;
	memory_region_add(&spu->region);

	/* Add SPU DMA channel */
	res = resource_get("dma",
		RESOURCE_DMA,
		instance->resources,
		instance->num_resources);
	spu->dma_channel.res = res;
	spu->dma_channel.ops = &spu_dma_ops;
	spu->dma_channel.data = spu;
	dma_channel_add(&spu->dma_channel);

	return true;
}

void spu_reset(struct controller_instance *instance)
{
	struct spu *spu = instance->priv_data;
	int i;

	/* Reset voice registers */
	for (i = 0; i < NUM_VOICES; i++) {
		spu->voice_vol_left[i].raw = 0;
		spu->voice_vol_right[i].raw = 0;
		spu->voice_adpcm_sample_rate[i] = 0;
		spu->voice_adpcm_start_addr[i] = 0;
		spu->voice_adsr[i].raw = 0;
		spu->voice_adsr_current_vol[i] = 0;
	}

	/* Reset remaining SPU registers */
	spu->main_vol_left.raw = 0;
	spu->main_vol_right.raw = 0;
	spu->reverb_output_vol_left = 0;
	spu->reverb_output_vol_right = 0;
	spu->voice_pitch_mode_en.raw = 0;
	spu->voice_noise_mode_en.raw = 0;
	spu->voice_reverb_mode_en.raw = 0;
	spu->snd_ram_data_addr = 0;
	spu->cnt.raw = 0;
	spu->stat.raw = 0;
	spu->cd_audio_input_vol_left = 0;
	spu->cd_audio_input_vol_right = 0;
	spu->ext_audio_input_vol_left = 0;
	spu->ext_audio_input_vol_right = 0;
	spu->snd_ram_reverb_work_area_start_addr = 0;
	spu->cur_snd_ram_data_addr = 0;
	spu->fifo.pos = 0;
	spu->fifo.num = 0;

	/* Reset SPU RAM */
	memset(spu->ram, 0, SPU_RAM_SIZE);
}

void spu_deinit(struct controller_instance *instance)
{
	free(instance->priv_data);
}

CONTROLLER_START(spu)
	.init = spu_init,
	.reset = spu_reset,
	.deinit = spu_deinit
CONTROLLER_END

