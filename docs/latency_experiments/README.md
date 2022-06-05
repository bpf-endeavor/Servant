# Latency Experiments

Latency experiments are done with MoonGen.


## For measuring Katran latency

Katran encapsulates response in ip-ip header.
For compliance with MoonGen timestamping scripts, there are small changes
to the Katran source code which prevents the encapsulation phase for PTP
packets (timestamped packets).

The diff file for changes are provided in this repository. You should apply
these patches to the source code before running experiments.


## Installing MoonGen

```
git clone https://github.com/emmericp/MoonGen
sudo apt-get install -y build-essential cmake linux-headers-`uname -r` pciutils libnuma-dev
# This is needed because for some reason builtin verion is not found by linker
sudo apt install -y libtbb2
cd MoonGen
./build.sh
```

## Tail latency


Moongen stores a histogram file of measured latencies values. `tail_latency.py` script
provided in this directory can analyse that file and report tail measurments.

