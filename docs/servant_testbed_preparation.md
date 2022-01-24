# About

This instructions are for preparing the cloudlab environment.

# Main Machine (Test machine)

1. Install Linux kernel 5.13 + rdtsc instructions

    ```bash
    cd /proj/progstack-PG0/farbod/ubuntu
    sudo dpkg -i *.deb
    ```
    
    > The packages for installing the kernel is available at ebpf-bench repo.
    
    Also enable huge pages. Edit `/etc/default/grub`.

    ```bash
    GRUB_CMDLINE_LINUX_DEFAULT="default_hugepagesz=1G hugepagesz=1G hugepages=16"
    ```

    then

    ```bash
    sudo update-grub
    sudo reboot
    ```

    After rebooting the 1GB huge pages are available and the kerne should be 5.13.
    
    ```bash
    uname -a 
    # Linux hostname 5.13.0-custom-ebpf-rdtsc #6 SMP Mon Sep 27 15:45:14 BST 2021 x86_64 x86_64 x86_64 GNU/Linux
    ```

3. 2. Install uBPF library

    ```bash
    git clone https://fyro.ir/fshahinfar1/uBPF.git
    cd uBPF/vm
    make
    sudo make install
    ```
    
    After these steps `libubpf` will be installed to the system.
    Other projects can link to it using `-lbpf` flag.

3. Install libbpf library

    ```bash
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

    ```bash
    # sudo apt update
    # sudo apt install -y clang-11 clang-9 llvm-11 llvm-9
    # Use llvm.sh script to install llvm-13
    ```
    > Install `llvm-13`. You need to get installation script from the website (also available in /docs directory).

    Update the `$HOME/.bashrc` and append this line. Then source the `bashrc` file.

    ```bash
    # Works with bash not sure if sh support this
    export CPATH=$CPATH:/usr/include/:/usr/include/x86_64-linux-gnu
    ```

    Next get the repository and build

    ```bash
    git clone https://fyro.ir/fshahinfar1/BMC
    cd BMC
    ./make
    ```

    This custom BMC repository has multiple branches. The main branch is a version
    that does not need the servant to operate.

5. Get the servant

    Clone the repository and compile it.

    ```bash
    git clone https://fyro.ir/fshahinfar1/Servant.git
    cd Servant/src
    make
    ```

6. Flow Steering

    ```
    sudo ethtool  -U enp24s0f1 flow-type udp4 dst-port 8080 action 2
    ```

7. Busypolling

    Use following script to enable/disable busypolling configuration.

    ```bash
    #! /bin/bash

    dev=enp24s0f1

    if [ "x$1" = "xoff" ]; then
        echo 0 | sudo tee /sys/class/net/$dev/napi_defer_hard_irqs
        echo 0 | sudo tee /sys/class/net/$dev/gro_flush_timeout
    else
        echo 2 | sudo tee /sys/class/net/$dev/napi_defer_hard_irqs
        echo 200000 | sudo tee /sys/class/net/$dev/gro_flush_timeout
    fi
    ```

8. Compiling lib-interpose

    Use `make` command in the `src/interpose` directory. It will need
    a version of `ubpf` that has been compiled with `-fPIC` flags.
    a diff of changes for `ubpf` make file to compile for this purpose
    is provided in this directory. apply the diff and make then install ubpf.

    **Note:** ubpf with `-fPIC` flag is not used in the runtime. It is
    only used in the lib-interpose.

For building the examples use `make` in their directory.

# Workload Generator machine

1. Setup Huge pages

    Edit `/etc/default/grub` and update these variables

    ```bash
    GRUB_CMDLINE_LINUX_DEFAULT="default_hugepagesz=1G hugepagesz=1G hugepages=16"
    ```

    Then

    ```bash
    sudo update-grub
    sudo reboot
    ```

2. Get DPDK

    ```bash
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

    ```bash
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

    ```bash
    git clone git://dpdk.org/dpdk-kmods
    cd dpdk-kmods/linux/igb_uio/
    make
    sudo modprobe uio
    sudo insmod /users/farbod/dpdk-kmods/linux/igb_uio/igb_uio.ko
    ```

6. Bind Script for DPDK

    > Take note of interface MAC address before binding!

    ```bash
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

1. If the experiment tool does not provide throughput information:

    ```bash
    ./mmwatch 'ethtool -S enp24s0f1 | egrep ".*: [1-9][0-9]*$"'
    ```

# Generating Load

## Mutilate

* Loading memcached server

> Old! pass load to then next script and it will do the job.

    ```bash
    #!/bin/bash

    ./mutilate/mutilate -s 192.168.1.1 --loadonly
    ```

* Stressing memcached server

> Script for running `mutilate` experiments

    ```bash
    #!/bin/bash

    ip=192.168.1.1
    port=8080
    conn=16
    duration=60

    recs=10000000
    # recs=10000
    # recs=100
    general="-K fb_key -V fb_value -r $recs --popularity=zipf:1.25"
    # general="-K 30 -V 200 -r $recs"

    bin=./mutilate/mutilateudp
    bin_loader=./mutilate/mutilate

    if [ "x$1" = "xload" ]; then
        # Load the database
        $bin_loader -s $ip --loadonly $general
    elif [ "x$1" = "xlow" ]; then
        # Low load for testing
        $bin -s $ip:$port -C 1 -c 1 --noload --time 5 $general
    elif [ "x$1" = "xlow2" ]; then
        # Low load and long duration for testing
        $bin -s $ip:$port -C 1 -c 1 --noload --time 60 $general
    else
        # Setup the Agent
        $bin -A -T 32 &
        sleep 1
        # Setup the Master
        $bin -s $ip:$port -C 1 -c $conn --noload -a 127.0.0.1 \
            --time $duration $general
    fi

    pkill mutilate
    pkill mutilateudp
    ```

