# How to work with benchmarks

for running the benchmarks use the following command:

	./run_benchmark.sh [ubpf|xdp] ./bin/[benchmark name].o

benchmark name is one of the following values:

1. memcpy
2. echo
3. adjust\_pkt\_size
4. ...

-- You may check the throughput using the `./bin/report_tput` utility (only for
-- the benchmarks that drop the packet).

> uBPF is not good with maps. In order to not interfere with its performance use the following command
`./mmwatch 'sudo ethtool -S enp24s0f1  | egrep "*: [1-9].*$"'`
