#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>

#include <servant/servant_engine.h>

int p1(struct pktctx *pkt) {
	DUMP("hello world from phase 1\n");
	return YIELD;
}

int p2(struct pktctx *pkt) {
	DUMP("At phase 2. Its a bit colder here\n");
	return DROP;
}
