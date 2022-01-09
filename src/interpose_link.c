#include <stdint.h>
#include "interpose_link.h"
#include "vchannel.h"
#include "log.h"

static struct vchannel vc;

int setup_interpose_link(void)
{
	static char channel_name[] = "rx_data_inject";
	struct channel_attr ch_attr = {
		.name = channel_name,
		.ring_size = 512,
	};
	int ret = connect_shared_channel(&ch_attr, &vc);
	if (ret) {
		ERROR("Failed in connecting to shared channel\n");
		return -1;
	}
	return 0;
}

int teardown_interpose_link(void)
{
	disconnect(&vc);
	return 0;
}

int send_interpose_msg(void *buf, uint32_t size)
{
	return vc_tx_msg(&vc, buf, size);
}
