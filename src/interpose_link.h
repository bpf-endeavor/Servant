#ifndef INTERPOSE_LINK_H
#define INTERPOSE_LINK_H

int setup_interpose_link(void);
int send_interpose_msg(void *buf, uint32_t size);
int teardown_interpose_link(void);

#endif
