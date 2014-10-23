#include <stdbool.h>
#include <stdlib.h>
#include <controller.h>
#include <log.h>
#include <port.h>
#include <util.h>
#include <video.h>

#define SCREEN_WIDTH		256
#define SCREEN_HEIGHT		192
#define SCREEN_REFRESH_RATE	60

struct vdp {
	struct port_region region;
};

static bool vdp_init(struct controller_instance *instance);
static void vdp_deinit(struct controller_instance *instance);
static uint8_t vdp_read(struct vdp *vdp, port_t port);
static void vdp_write(struct vdp *vdp, uint8_t b, port_t port);

static struct pops vdp_pops = {
	.read = (read_t)vdp_read,
	.write = (write_t)vdp_write
};

uint8_t vdp_read(struct vdp *UNUSED(vdp), port_t port)
{
	LOG_D("vdp_read (%02x)\n", port);
	return 0;
}

void vdp_write(struct vdp *UNUSED(vdp), uint8_t b, port_t port)
{
	LOG_D("vdp_write (%02x, %02x)\n", b, port);
}

bool vdp_init(struct controller_instance *instance)
{
	struct vdp *vdp;
	struct video_specs video_specs;
	struct resource *res;

	/* Initialize video frontend */
	video_specs.width = SCREEN_WIDTH;
	video_specs.height = SCREEN_HEIGHT;
	video_specs.fps = SCREEN_REFRESH_RATE;
	if (!video_init(&video_specs))
		return false;

	/* Allocate VDP structure */
	instance->priv_data = malloc(sizeof(struct vdp));
	vdp = instance->priv_data;

	/* Add VDP port region */
	res = resource_get("port",
		RESOURCE_PORT,
		instance->resources,
		instance->num_resources);
	vdp->region.area = res;
	vdp->region.pops = &vdp_pops;
	vdp->region.data = vdp;
	port_region_add(&vdp->region);

	return true;
}

void vdp_deinit(struct controller_instance *instance)
{
	video_deinit();
	free(instance->priv_data);
}

CONTROLLER_START(vdp)
	.init = vdp_init,
	.deinit = vdp_deinit
CONTROLLER_END

