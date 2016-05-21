#ifndef _PSX_CONTROLLER_H
#define _PSX_CONTROLLER_H

struct psx_peripheral {
	void *priv_data;
	int port;
	bool (*send)(struct psx_peripheral *peripheral, uint8_t *data);
	void (*receive)(struct psx_peripheral *peripheral, uint8_t data);
};

void psx_ctrl_add(struct controller_instance *i, struct psx_peripheral *p);

#endif

