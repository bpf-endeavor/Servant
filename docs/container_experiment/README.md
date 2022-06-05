# Conatiner Experiments

Experimenting with applications inside the containers has the potential of
showing a case for keep program logic inside the kernel.

At the time we have shown that there exists some useful cases for bring XDP
program to user-space. We are investigating the cases/reasons why someone may
want to avoid going to the user-space.

For this tests I tried to connect BMC to docker containers.

## Postmortem notes:

For this task I need to connect XDP programs to veth interface of a docker
container. For some reason connecting XDP programs in native mode is not
working. There is an issue when the program returns XDP\_TX.  The transimitted
packet is not received by the container. Connecting XDP program with
XDP-Generic does work.

I can not compare the results of XDP programs (connected to the generic hook)
with user-space implementation because veth driver does not support AF\_XDP.

> One might ask the question, why should there be an AF\_XDP support for veth
driver? Should anyone implement such feature?
