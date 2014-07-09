#include <log.h>
#include <machine.h>
#include <util.h>

static bool sms_init(struct machine *machine);
static void sms_deinit(struct machine *machine);

bool sms_init(struct machine *UNUSED(machine))
{
	return true;
}

void sms_deinit(struct machine *UNUSED(machine))
{
}

MACHINE_START(sms, "Sega Master System")
	.init = sms_init,
	.deinit = sms_deinit
MACHINE_END

