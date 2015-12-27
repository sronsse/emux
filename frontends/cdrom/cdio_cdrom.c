#include <cdio/cdio.h>
#include <cdio/logging.h>
#include <cdio/util.h>
#include <cdrom.h>
#include <log.h>
#include <util.h>

static bool libcdio_init(struct cdrom_frontend *fe, char *source);
static void libcdio_deinit(struct cdrom_frontend *fe);
static struct msf libcdio_get_msf_from_sector(struct cdrom_frontend *fe, int s);
static struct msf libcdio_get_msf_from_track(struct cdrom_frontend *fe, int t);
static int libcdio_get_sect_from_msf(struct cdrom_frontend *, struct msf *);
static int libcdio_get_track_from_sector(struct cdrom_frontend *fe, int sector);
static int libcdio_get_first_track_num(struct cdrom_frontend *fe);
static int libcdio_get_last_track_num(struct cdrom_frontend *fe);
static bool libcdio_read_sector(struct cdrom_frontend *fe, void *buf, int sect,
	enum cdrom_read_mode read_mode);
static void libcdio_log(cdio_log_level_t level, const char *message);

struct cdio_data {
	CdIo_t *cdio;
};

void libcdio_log(cdio_log_level_t level, const char *message)
{
	/* Print libcdio's message based on log level */
	switch (level) {
	case CDIO_LOG_DEBUG:
		LOG_D("cdio: %s\n", message);
		break;
	case CDIO_LOG_INFO:
		LOG_I("cdio: %s\n", message);
		break;
	case CDIO_LOG_WARN:
		LOG_W("cdio: %s\n", message);
		break;
	case CDIO_LOG_ERROR:
	case CDIO_LOG_ASSERT:
	default:
		LOG_E("cdio: %s\n", message);
		break;
	}
}

struct msf libcdio_get_msf_from_sector(struct cdrom_frontend *UNUSED(fe), int s)
{
	struct msf msf;
	msf_t v;

	/* Convert logical sector number and return filled MSF structure */
	cdio_lsn_to_msf(s, &v);
	msf.m = cdio_from_bcd8(v.m);
	msf.s = cdio_from_bcd8(v.s);
	msf.f = cdio_from_bcd8(v.f);
	return msf;
}

struct msf libcdio_get_msf_from_track(struct cdrom_frontend *fe, int track)
{
	struct cdio_data *data = fe->priv_data;
	struct msf msf;
	msf_t v;

	/* Convert track number and return filled MSF structure */
	cdio_get_track_msf(data->cdio, track, &v);
	msf.m = cdio_from_bcd8(v.m);
	msf.s = cdio_from_bcd8(v.s);
	msf.f = cdio_from_bcd8(v.f);
	return msf;
}


int libcdio_get_sect_from_msf(struct cdrom_frontend *UNUSED(fe), struct msf *v)
{
	msf_t msf;
	lsn_t lsn;

	/* Convert MSF value and return logical sector number */
	msf.m = cdio_to_bcd8(v->m);
	msf.s = cdio_to_bcd8(v->s);
	msf.f = cdio_to_bcd8(v->f);
	lsn = cdio_msf_to_lsn(&msf);
	return lsn;
}

int libcdio_get_track_from_sector(struct cdrom_frontend *fe, int sect)
{
	struct cdio_data *data = fe->priv_data;
	int track;

	/* Get track number from logical sector number and return it */
	track = cdio_get_track(data->cdio, sect);
	return track;
}

int libcdio_get_first_track_num(struct cdrom_frontend *fe)
{
	struct cdio_data *data = fe->priv_data;
	track_t track;

	/* Get first track number and return it */
	track = cdio_get_first_track_num(data->cdio);
	return track;
}

int libcdio_get_last_track_num(struct cdrom_frontend *fe)
{
	struct cdio_data *data = fe->priv_data;
	track_t track;

	/* Get last track number and return it */
	track = cdio_get_last_track_num(data->cdio);
	return track;
}

bool libcdio_read_sector(struct cdrom_frontend *fe, void *buf, int sect,
	enum cdrom_read_mode read_mode)
{
	struct cdio_data *data = fe->priv_data;
	cdio_read_mode_t mode;

	/* Adapt read mode */
	switch (read_mode) {
	case CDROM_READ_MODE_AUDIO:
		mode = CDIO_READ_MODE_AUDIO;
		break;
	case CDROM_READ_MODE_M1F1:
		mode = CDIO_READ_MODE_M1F1;
		break;
	case CDROM_READ_MODE_M1F2:
		mode = CDIO_READ_MODE_M1F2;
		break;
	case CDROM_READ_MODE_M2F1:
		mode = CDIO_READ_MODE_M2F1;
		break;
	case CDROM_READ_MODE_M2F2:
	default:
		mode = CDIO_READ_MODE_M2F2;
		break;
	}

	/* Read requested sector based on mode */
	if (cdio_read_sector(data->cdio, buf, sect, mode)) {
		LOG_E("cdio: could not read sector %u!\n", sect);
		return false;
	}

	/* Return read success */
	return true;
}

bool libcdio_init(struct cdrom_frontend *fe, char *source)
{
	struct cdio_data *data;

	/* Allocate private data */
	data = calloc(1, sizeof(struct cdio_data));
	fe->priv_data = data;

	/* Set libcdio log level to ours */
	switch (log_level) {
	case LOG_DEBUG:
		cdio_loglevel_default = CDIO_LOG_DEBUG;
		break;
	case LOG_INFO:
		cdio_loglevel_default = CDIO_LOG_INFO;
		break;
	case LOG_WARNING:
		cdio_loglevel_default = CDIO_LOG_WARN;
		break;
	case LOG_ERROR:
	default:
		cdio_loglevel_default = CDIO_LOG_ERROR;
		break;
	}

	/* Install custom log handler */
	cdio_log_set_handler(libcdio_log);

	/* Open instance */
	data->cdio = cdio_open(source, DRIVER_UNKNOWN);
	if (!data->cdio) {
		LOG_E("cdio: could not open source \"%s\"!\n", source);
		free(data);
		return false;
	}

	return true;
}

void libcdio_deinit(struct cdrom_frontend *fe)
{
	struct cdio_data *data = fe->priv_data;
	cdio_destroy(data->cdio);
	free(data);
}

CDROM_START(cdio)
	.init = libcdio_init,
	.get_msf_from_sector = libcdio_get_msf_from_sector,
	.get_msf_from_track = libcdio_get_msf_from_track,
	.get_sector_from_msf = libcdio_get_sect_from_msf,
	.get_track_from_sector = libcdio_get_track_from_sector,
	.get_first_track_num = libcdio_get_first_track_num,
	.get_last_track_num = libcdio_get_last_track_num,
	.read_sector = libcdio_read_sector,
	.deinit = libcdio_deinit
CDROM_END

