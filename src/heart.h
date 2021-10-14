#ifndef HEART_H
#define HEART_H
#include "defs.h"
#include "ubpf.h"
void pump_packets(struct xsk_socket_info *xsk, struct ubpf_vm *vm);
#endif
