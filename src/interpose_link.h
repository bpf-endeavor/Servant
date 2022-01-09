#ifndef INTERPOSE_LINK_H
#define INTERPOSE_LINK_H

#include "vchannel.h"

int setup_interpose_link(void);
int setup_interpose_vchannel(struct vchannel *vc);
int send_interpose_msg(void *buf, uint32_t size);
int teardown_interpose_link(void);

#endif
