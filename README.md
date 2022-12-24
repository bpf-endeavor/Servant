# Servant

Servant combines uBPF and AF\_XDP. Using it you can write eBPF packet
processing programs that run in usersapce.


## Compile

**Install the dependencies:**

* Tested on: Linux kernel 5.13
* uBPF (custom version)
* libbpf (commit: f9f6e92458899fee5d3d6c62c645755c25dd502d)
* llvm 13

You can look at `/setup.sh`


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
| [map] | name of the in kernel map which eBPF program want to access you can also assign the map an index using "name:index". This index could be used with `lookup_fast` helper function.|
| [core] | pin the process to the given CPU core |
| [xdp-prog] | path to the XDP program. this program should have a map of type `BPF_MAP_TYPE_XSKMAP` named `xsks_map`|
| [busypoll] | run AF\_XDP in busy-polling mode |
| [...] | there are other options (to be complete) |

## Run an example program

There are some example programs at `/examples`.

```bash
# map incomming ipv4/udp traffic with destination port of 8080 to queue 2
sudo ethtool -U <iface> flow-type udp4 dst-port 8080 action 2
cd ./examples/dump_packet
make
# Run the eBPF program with an AF_XDP socket connected on queue 2
sudo ../../src/servant <ifce> 2 ./ubpf.o
```

Generate traffic toward the uBPF program. The packets should be displayed on the
terminal.

On the other machine start a netcat and send UDP traffic.

```bash
nc -u <ip address of first machine> 8080
hello
```

Out put of uBPF program should be like below

```
...
Packet size: 60
Src MAC: 3c:fd:fe:56:1:a2
Dest MAC: 3c:fd:fe:55:ff:42
Ether Type: 800
Src IP: c0a80102
Dst IP: c0a80101
Transport: UDP
Src PORT: 57300
Dst PORT: 8080
...
```

## Example eBPF Program


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

> Currently destination of packets injected to the kernel (`PASS`) are hard-coded but this can be solved and is not a limitation.
> Look at `/src/udp_sock.c`


## eBPF Program Helpers

For list of helper functions defined look at `/src/include/servant_engine.h`

| Helper | Description |
|--------|-------------|
| `lookup` | lookup an in kernel map using name of the map |
| `lookup_fast` | lookup an in kernel map using index of the map (You can define index of each map when starting Servant) |
| `userspace_lookup` | lookup a userspace map |
| `userspace_update` | update a usersapce map |
| `ubpf_get_time_ns` | read clock MONOTONIC value |
| `ubpf_print` | print messages |
