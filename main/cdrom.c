#include <string.h>
#include <cdrom.h>
#include <cmdline.h>
#include <env.h>
#include <list.h>
#include <log.h>

/* Command-line parameters */
static char *cdrom_fe_name;
PARAM(cdrom_fe_name, string, "cdrom", NULL, "Selects CD-ROM frontend")

struct list_link *cdrom_frontends;
static struct cdrom_frontend *frontend;

bool cdrom_init(char *source)
{
	struct list_link *link = cdrom_frontends;
	struct cdrom_frontend *fe;

	if (frontend) {
		LOG_E("CD-ROM frontend already initialized!\n");
		return false;
	}

	/* Validate CD-ROM option */
	if (!cdrom_fe_name) {
		LOG_E("No CD-ROM frontend selected!\n");
		return false;
	}

	/* Find CD-ROM frontend */
	while ((fe = list_get_next(&link))) {
		/* Skip if name does not match */
		if (strcmp(cdrom_fe_name, fe->name))
			continue;

		/* Initialize frontend */
		if (fe->init && !fe->init(fe, source))
			return false;

		/* Save frontend and return success */
		frontend = fe;
		return true;
	}

	/* Warn as CD-ROM frontend was not found */
	LOG_E("CD-ROM frontend \"%s\" not recognized!\n", cdrom_fe_name);
	return false;
}

struct msf cdrom_get_msf_from_sector(int sector)
{
	struct msf msf = { 0 };

	if (frontend && frontend->get_msf_from_sector)
		msf = frontend->get_msf_from_sector(frontend, sector);

	return msf;
}

struct msf cdrom_get_msf_from_track(int track)
{
	struct msf msf = { 0 };

	if (frontend && frontend->get_msf_from_track)
		msf = frontend->get_msf_from_track(frontend, track);

	return msf;
}

int cdrom_get_sector_from_msf(struct msf *msf)
{
	int sector = 0;

	if (frontend && frontend->get_sector_from_msf)
		sector = frontend->get_sector_from_msf(frontend, msf);

	return sector;
}

int cdrom_get_track_from_sector(int sector)
{
	int track = 0;

	if (frontend && frontend->get_track_from_sector)
		track = frontend->get_track_from_sector(frontend, sector);

	return track;
}

int cdrom_get_first_track_num()
{
	if (frontend && frontend->get_first_track_num)
		return frontend->get_first_track_num(frontend);
	return 0;
}

int cdrom_get_last_track_num()
{
	if (frontend && frontend->get_last_track_num)
		return frontend->get_last_track_num(frontend);
	return 0;
}

bool cdrom_read_sector(void *buf, int sect, enum cdrom_read_mode read_mode)
{
	if (frontend && frontend->read_sector)
		return frontend->read_sector(frontend, buf, sect, read_mode);
	return false;
}

void cdrom_deinit()
{
	if (!frontend)
		return;

	if (frontend->deinit)
		frontend->deinit(frontend);
	frontend = NULL;
}

