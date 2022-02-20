# About

Installing Katran and preparing the environment for experiments.

> Assuming the Servant has been installed (consult with servant_testbed_preparation.md).

# Installing Katran

* Note: On cloudlab use one of the experiment storages. After compiling katran
uses 12 GB of storage

```
sudo lsblk
sudo mkfs.ext4 /dev/...
sudo mkdir /media/disk
sudo mount /dev/... /media/disk
sudo chown <user> /media/disk
cd /media/disk
```

Clone the repository and run the build script.
For running the grpc example install:
	* go
	* protobuf compiler
	* go modules
Then run the build script.

```
git clone https://fshahinfar1@fyro.ir/fshahinfar1/katran
cd katran/notes
./go_deps.sh
cd ../
./build_katran.sh
```

## Installing dependencies

> The script below is available at `katran/notes/go_deps.sh`

Install `Go`, `protoc`, and needed `Go` modules

```bash
#! /bin/bash
# Installing Go
TARFILE=go1.17.6.linux-amd64.tar.gz
wget https://go.dev/dl/$TARFILE
sudo rm -rf /usr/local/go && sudo tar -C /usr/local -xzf $TARFILE
echo 'export PATH=$PATH:/usr/local/go/bin' | sudo tee -a $HOME/.profile
source $HOME/.profile
echo "Testing if go is setup:"
go version
if [! $? -eq 0 ]; then
    echo "Failed to setup Go"
    exit 1
fi
# Protobuf compiler
echo  "Installing protoc"
sudo apt-get update
sudo apt-get install -y protobuf-compiler
echo "Testing protoc:"
protoc --version
if [! $? -eq 0 ] ; then
    echo "Failed to install protoc"
    exit 1
fi
# Install Go modules
go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.26
go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@v1.1
go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest
go get -u google.golang.org/grpc
echo 'export PATH="$PATH:$(go env GOPATH)/bin"' | sudo tee -a $HOME/.profile
source $HOME/.profile
```

# Running Katran

I prepared a script for running the katran server and configuring

```bash
#! /bin/bash

other_ip="192.168.1.1"

katran_dir="/media/disk/katran"
build_dir="$katran_dir/_build"
lb="$build_dir/deps/bpfprog/bpf/balancer_kern.o"
grpc_server="$build_dir/build/example_grpc/katran_server_grpc"
grpc_client="$katran_dir/example_grpc/goclient/bin/main"

t=`sysctl net.core.bpf_jit_enable | awk '{print $3}'`
if [ ! t -eq 1 ]; then
	echo "eBPF jit is not enable !"
	echo "\t sysctl net.core.bpf_jit_enable=1"
	exit 1
fi

# Interface selection
ping -c 3  $other_ip
default_gateway=`ip n show | grep $other_ip | awk '{print $5}'`
echo "Selected Default Gatway: $default_gateway"
interface=`ip n show | grep $other_ip | awk '{print $3}'`

# Interrupt pinning
# count_core=`nproc`
count_core=4
sudo ethtool -L $interface combined $count_core
sudo systemctl stop irqbalance
irqs=`cat /proc/interrupts | grep enp24s0f1 | awk '{printf("%d ", substr($1,1, length($1)-1))} END {print ""}'`
core=1
selected_cores=()
for i in $irqs; do
	echo "Intrrupt: $i"
	sudo  sh -c "echo $core > /proc/irq/$i/smp_affinity"
	selected_cores+=(`echo "$core-1" | bc`)
	core=$((core+2))
done
echo "Selected cores: ${selected_cores[@]}"
for core in ${selected_cores[@]}; do
	t=`cat /sys/devices/system/cpu/cpu$core/topology/physical_package_id`
	if [ ! $t -eq 0 ]; then
		echo "Core $core is not on NUMA node zero (NUMA: $t)"
		exit 1
	fi
done
echo "Checked NUMA nodes"
forward_cores=`echo ${selected_cores[@]} | tr ' ' ','`
num_nodes=`echo $forward_cores | tr '[1-9]' '0'`
# CPU parameter
param="-balancer_prog $lb -default_mac $default_gateway \
	-forwarding_cores=$forward_cores \
	-numa_nodes=$num_nodes -hc_forwarding=false \
	-intf=$interface -ipip_intf=ipip0 -ipip6_intf=ipip60 \
	-lru_size=100000"

echo $param

# Setup IPIP
sudo ip link add name ipip0 type ipip external
sudo ip link add name ipip60 type ip6tnl external
sudo ip link set up dev ipip0
sudo ip link set up dev ipip60

sudo $grpc_server $param
```

configuring the server

```bash
#! /bin/bash

katran_dir="/media/disk/katran"
grpc_client="$katran_dir/example_grpc/goclient/bin/main"

vip_ip="192.168.1.10"
vip_port=8080
vip="$vip_ip:$vip_port"
real="192.168.1.2"

sudo ip addr add $vip_ip/32 dev lo

$grpc_client -A -t $vip
$grpc_client -a -t $vip -r $real
$grpc_client -l
$grpc_client -s -lru
```

# Testing if katran is working

I used `netcat` for this purpose

on the real server run a listening `nc`

```
> nc -l 8080
```

Run the katran server and configuration scrip.
then from the clien server initiate the connection and send messages

```
> nc <vip_ip> <vip_port>
Hello
```


# Development mode

I use `build_katran.sh` script to build the system after my changes.
There should be a faster way but I did not put time on understanding
the build system of this project.


# Katran + Servant (spliting to Userspace)

* Enable busy polling by running the configuration script.
* Run Katran server (single core).
* Configure server.
* Run servant:

```
sudo taskset -c 0 ./servant --xdp-prog - --busypoll enp24s0f1 0 /media/disk/my/katran/_build/deps/bpfprog/bpf/ubpf.o
```

* Run load generator on the other machine.


# Load generator

I used dpdk pktgen. Look servant notes on how to install it.

command

```bash
sudo pktgen -l 0,2,4,6 -n 4 -a 18:00.1 --file-prefix pg -- -m "[2:4].0" -T -P
```

configuring pktgen

```
set 0 size 1500
set 0 burst 32
set 0 sport 8080
set 0 dport 8080
set 0 src mac 3c:fd:fe:55:df:62
set 0 dst mac 3c:fd:fe:55:e1:62
set 0 type ipv4
set 0 proto udp
set 0 src ip 192.168.1.1/32
set 0 dst ip 192.168.1.10
```

configure the rate

```
set 0 rate 100
```

run

```
start 0
stop 0
```

