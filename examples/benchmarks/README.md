# How to work with benchmarks

for running the benchmarks use the following command:

	./run_benchmark.sh [ubpf|xdp] ./bin/[benchmark name].o

benchmark name is one of the following values:

1. memcpy
2. echo
3. adjust\_pkt\_size

You may check the throughput using the `./bin/report_tput` utility (only for
the benchmarks that drop the packet).
