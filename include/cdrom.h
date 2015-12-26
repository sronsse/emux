#ifndef _CDROM_H
#define _CDROM_H

#include <stdbool.h>
#include <list.h>

#define CDROM_START(_name) \
	static struct cdrom_frontend _cdrom_frontend = { \
		.name = #_name,
#define CDROM_END \
	}; \
	__attribute__((constructor)) static void _register() \
	{ \
		list_insert(&cdrom_frontends, &_cdrom_frontend); \
	} \
	__attribute__((destructor)) static void _unregister() \
	{ \
		list_remove(&cdrom_frontends, &_cdrom_frontend); \
	}

#define CDROM_CD_DA_SECTOR_SIZE	2352
#define CDROM_M1F1_SECTOR_SIZE	2048
#define CDROM_M1F2_SECTOR_SIZE	2336
#define CDROM_M2F1_SECTOR_SIZE	2048
#define CDROM_M2F2_SECTOR_SIZE	2328

enum cdrom_read_mode {
	CDROM_READ_MODE_AUDIO,
	CDROM_READ_MODE_M1F1,
	CDROM_READ_MODE_M1F2,
	CDROM_READ_MODE_M2F1,
	CDROM_READ_MODE_M2F2
};

struct msf {
	int m;
	int s;
	int f;
};

typedef void cdrom_priv_data_t;

struct cdrom_frontend {
	char *name;
	cdrom_priv_data_t *priv_data;
	bool (*init)(struct cdrom_frontend *fe, char *source);
	struct msf (*get_msf_from_sector)(struct cdrom_frontend *fe, int sect);
	struct msf (*get_msf_from_track)(struct cdrom_frontend *fe, int track);
	int (*get_sector_from_msf)(struct cdrom_frontend *fe, struct msf *msf);
	int (*get_track_from_sector)(struct cdrom_frontend *fe, int sect);
	int (*get_first_track_num)(struct cdrom_frontend *fe);
	int (*get_last_track_num)(struct cdrom_frontend *fe);
	bool (*read_sector)(struct cdrom_frontend *fe, void *buf, int sect,
		enum cdrom_read_mode read_mode);
	void (*deinit)(struct cdrom_frontend *fe);
};

bool cdrom_init(char *source);
struct msf cdrom_get_msf_from_sector(int sector);
struct msf cdrom_get_msf_from_track(int track);
int cdrom_get_sector_from_msf(struct msf *msf);
int cdrom_get_track_from_sector(int sector);
int cdrom_get_first_track_num();
int cdrom_get_last_track_num();
bool cdrom_read_sector(void *buf, int sect, enum cdrom_read_mode read_mode);
void cdrom_deinit();

extern struct list_link *cdrom_frontends;

#endif

