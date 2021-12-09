# About
This instructions are for preparing the cloudlab environment.

# Main Machine (Test machine)
1. Install Linux kernel 13 + rdtsc instructions

```
cd /proj/progstack-PG0/farbod/ubuntu
sudo dpkg -i *.deb
sudo reboot
```

2. Install uBPF library

```
git clone https://fyro.ir/fshahinfar1/uBPF.git
cd uBPF/vm
make
sudo make install
```

3. Install libbpf library

```
# Some dependencies you might need
sudo apt update
sudo apt install -y libz-dev libelf-dev
# Getting and compiling the libbpf
git clone https://github.com/libbpf/libbpf
cd libbpf
git checkout f9f6e92458899fee5d3d6c62c645755c25dd502d
cd src
mkdir build
make all OBJDIR=.
# make install_headers DESTDIR=build OBJDIR=.
make install DESTDIR=build OBJDIR=.
sudo make install
# Update ldconfig files
echo `whereis libbpf` | awk '{print $2}' | xargs dirname | sudo tee /etc/ld.so.conf.d/libbpf.conf
sudo ldconfig
```

4. Get Modified BMC repository

First install some dependenceis.

```
sudo apt update
sudo apt install -y clang-11 clang-9 llvm-11 llvm-9
```

Update the `$HOME/.bashrc` and append this line. Then source the `bashrc` file.

```
# Works with bash not sure if sh support this
export CPATH=$CPATH:/usr/include/:/usr/include/x86_64-linux-gnu
```

Next get the repository and build

```
git clone https://fyro.ir/fshahinfar1/BMC
cd BMC
./make
```

This custom BMC repository has multiple branches. The main branch is a version
that does not need the servant to operate.

5. Get the servant

First enable huge pages on the machine. Edit `/etc/default/grub`.

```
GRUB_CMDLINE_LINUX_DEFAULT="default_hugepagesz=1G hugepagesz=1G hugepages=16"
```

then

```
sudo update-grub
sudo reboot
```

After rebooting the 1GB huge pages are available. Clone the repository and
compile it.

```
git clone https://fyro.ir/fshahinfar1/Servant.git
cd Servant/src
make
```

6. Flow Steering

```
sudo ethtool  -U enp24s0f1 flow-type udp4 dst-port 8080 action 2
```

For building the examples use `make` in their directory.

# Workload Generator machine

1. Setup Huge pages

Edit `/etc/default/grub` and update these variables

```
GRUB_CMDLINE_LINUX_DEFAULT="default_hugepagesz=1G hugepagesz=1G hugepages=16"
```

Then

```
sudo update-grub
sudo reboot
```

2. Get DPDK

```
sudo apt update
sudo apt install meson ninja-build python3-pyelftools libpcap-dev
git clone git://dpdk.org/dpdk
cd dpdk
git checkout v21.11
meson build
cd build
ninja
sudo ninja install
```

3. Get pktgen

```
sudo apt install libnuma-dev
git clone git://dpdk.org/apps/pktgen-dpdk
cd pktgen-dpdk
git checkout pktgen-21.11.0
meson build
cd build
ninja
sudo ninja install
```

4. Get custom version of Mutilate

Get the install script and run it.

5. Get DPDK `igb_uio` Kernel Module

```
git clone git://dpdk.org/dpdk-kmods
cd dpdk-kmods/linux/igb_uio/
make
sudo modprobe uio
sudo insmod /users/farbod/dpdk-kmods/linux/igb_uio/igb_uio.ko
```

6. Bind Script for DPDK

> Take note of interface MAC address before binding!

```
#! /bin/bash

INTERFACE=enp24s0f1
PCI=18:00.1
DPDK_DRIVER=igb_uio
NORMAL_DRIVER=i40e
IP="192.168.1.2/24"

if [ "x$1" = "xunbind" ]; then
	sudo dpdk-devbind.py -u $PCI
	sudo dpdk-devbind.py -b $NORMAL_DRIVER $PCI
	sudo ip link set dev $INTERFACE up
	sudo ip addr add $IP dev $INTERFACE
else
	sudo ip link set dev $INTERFACE down
	sudo dpdk-devbind.py -u $PCI
	sudo dpdk-devbind.py -b $DPDK_DRIVER $PCI
fi
```

# Measurments

On the main machine

```
./mmwatch 'ethtool -S enp24s0f1 | egrep ".*: [1-9][0-9]*$"'
```

# Generating Load

## Mutilate

* Loading memcached server

```
#!/bin/bash

./mutilate/mutilate -s 192.168.1.1 --loadonly
```

* Stressing memcached server

```
#!/bin/bash

bin=./mutilate/mutilateudp

# Setup the Agent
$bin -A -T 32 &> /dev/null &
sleep 1

# Setup the Master

$bin -s 192.168.1.1:8080 -C 1 -c 4 --noload -a 127.0.0.1 --time 30

pkill mutilate
```

