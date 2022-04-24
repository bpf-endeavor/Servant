# Latency Experiments

Latency experiments are done with MoonGen.


## For measuring Katran latency

Katran encapsulates response in ip-ip header.
For compliance with MoonGen timestamping sripts, there are small changes
to the Katran source code which prevents the encapsulation phase for PTP
packets (timestamped packets).

The diff file for changes are provided in this repository. You should apply
these patches to the source code before running experiments.


## Installing MoonGen

> To be complete
