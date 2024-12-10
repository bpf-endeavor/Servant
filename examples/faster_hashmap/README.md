# What is this

This example (`ubpf.c`) demonstrates the overhead of cache-miss for stateful packet processing software.
Especially it demonstrates cost of using hash-maps. The program experiences
about LLC miss for approximately 50% of accesses.

The `ubpf_p1.c` and `ubpf_p2.c` is a version of the same program trying to
overcome the cache-miss issue by getting some help from the runtime to overlap
loading the values with processing other packets.
