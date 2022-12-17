# Servant

Servant combines uBPF and AF\_XDP. Using it you can write eBPF packet
processing programs that run in usersapce.


## Compile

**Install the dependencies:**

* Tested on: Linux kernel 5.13
* uBPF (custom version)
* libbpf (commit: f9f6e92458899fee5d3d6c62c645755c25dd502d)
* llvm 13

You can look at `/docs/setup_cloudlab.sh`


**Compile:**

use `make` at `/src/` to compile the Servant.
You can install Servant's header with `make install`.


## Usage

> Enable the Huge page size support.

*Servant Arguments:*

| Arg | Description |
|-----|-------------|
| ifname | name of the ethernet interface to attach |
| qid | queue index for attaching AF\_XDP |
| ebpf | path to the ebpf program binary to load |
| [map] | name of the in kernel map which eBPF program want to access you can also assign the map an index using "name:idnex". This index could be used with `lookup_fast` helper function.|
| [core] | pin the process to the given CPU core |
| [...] | there are other options (to be complete) |

## Example eBPF Program

There are some examples at `/src/examples`.

The structure of the program looks like below.

```c
#include <servant/servant_engine.h>

int bpf_prog(struct pktctx *ctx)
{
	return DROP;
}
```

**struct pktctx:**

Each eBPF program will receive packets in the following form.
(look at `/src/include/packet_context.h`)

```c
struct pktctx {
	void *data; // in: start of the packet
	void *data_end; // in: end of the packet
	uint32_t pkt_len; // inout: final length of packet
	int32_t trim_head; // out: skip n bytes from the head before sending
};
```

**return codes:**

The eBPF program should return one of the following values.

1. `PASS`: Pass the packet to the network stack.
1. `DROP`: Drop the packet.
1. `SEND`: Send the packet on the attached ethernet interface.

> Currently destination of packets injected to the kenrel (`PASS`) are hardcoded but this can be solved and is not a limitation.
> Look at `/src/udp_sock.c`


## eBPF Program Helpers

For list of heper functions defined look at `/src/include/servant_engine.h`

| Helper | Description |
|--------|-------------|
| `lookup` | lookup an in kernel map using name of the map |
| `lookup_fast` | lookup an in kernel map using index of the map (You can define index of each map when starting Servant) |
| `userspace_lookup` | lookup a userspace map |
| `userspace_update` | update a usersapce map |
| `ubpf_get_time_ns` | read clock MONOTONIC value |
| `ubpf_print` | print messages |

## From Userspace to Kernel

For allowing the eBPF program to send the packet back to the userspace
