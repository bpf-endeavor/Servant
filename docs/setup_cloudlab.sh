#! /bin/bash

sudo echo "hello :D"

usage() {
	echo "Usage: $0 [DUT/WORKLOAD]"
}

# Check argv
if [ $# -lt 1 ]; then
	usage
	exit 1
fi
MODE="WORKLOAD"
if [ $1 = "DUT" ]; then
	MODE="DUT"
elif [ $1 = "WORKLOAD" ]; then
	MODE="WORKLOAD"
else
	usage
	exit 1
fi

# CHeck dummy key is added (for private repo access)
if [ ! -f "$HOME/.ssh/id_dummy" ]; then
	echo "SSH key for accessing private repos is not installed"
	exit 1
fi

# Install htop
sudo apt update
sudo apt install -y htop

# Configure tmux
echo 'set -g default-terminal "xterm-256color"' | tee -a $HOME/.tmux.conf

# Configure git
git config --global core.editor vim
git config --global user.name "Farbod Shahinfar"
git config --global user.email "fshahinfar1@gmail.com"

# Configure vim
cd $HOME
git clone https://github.com/fshahinfar1/dotvim
cd $HOME/dotvim
./install.sh
cd $HOMEM

# Configure HUGEPAGES
grub='GRUB_CMDLINE_LINUX_DEFAULT="default_hugepagesz=1G hugepagesz=1G hugepages=16"'
echo $grub | sudo tee -a /etc/default/grub
sudo update-grub

if [ $MODE = "DUT" ]; then
	# # install kernel 5.13
	cd /proj/progstack-PG0/farbod/ubuntu
	sudo dpkg -i *.deb
	ubpf
	cd $HOME
	git clone ssh://git@fyro.ir:10022/fshahinfar1/uBPF.git --config core.sshCommand='ssh -i ~/.ssh/id_dummy'
	cd uBPF/vm
	make
	sudo make install
	# libbpf
	cd $HOME
	sudo apt install -y libz-dev libelf-dev pkg-config
	git clone https://github.com/libbpf/libbpf
	cd libbpf
	git checkout f9f6e92458899fee5d3d6c62c645755c25dd502d
	cd src
	mkdir build
	make all OBJDIR=.
	make install DESTDIR=build OBJDIR=.
	sudo make install
	echo `whereis libbpf` | awk '{print $2}' | xargs dirname | sudo tee /etc/ld.so.conf.d/libbpf.conf
	sudo ldconfig
	# servant
	cd $HOME
	git clone ssh://git@fyro.ir:10022/fshahinfar1/Servant.git --config core.sshCommand='ssh -i ~/.ssh/id_dummy'
	cd Servant
	git submodule update --init
	cd src
	make
	sudo make install
	# llvm13
	cd $HOME/Servant/docs
	sudo bash llvm.sh
	# llvm9
	sudo apt install -y clang-9
	# BMC
	cd $HOME
	git clone ssh://git@fyro.ir:10022/fshahinfar1/BMC.git --config core.sshCommand='ssh -i ~/.ssh/id_dummy'
	cd BMC
	./make.sh
	# Copy mmwatch
	cp $Home/Servant/docs/mmwatch $Home/
	# Userspace Katran
	cd $HOME
	git clone ssh://git@fyro.ir:10022/fshahinfar1/katran_userspace.git --config core.sshCommand='ssh -i ~/.ssh/id_dummy'
	cd katran_userspace
	make
	# Perf
	cd $HOME
	sudo apt install -y libzstd-dev libunwind-dev libperl-dev libnuma-dev libcap-dev libbfd-dev libdwarf-dev libslang2-dev
	tar -xf /proj/progstack-PG0/farbod/linux-5.13.tar.xz
	cd linux-5.13/tools/perf/
	make
	sudo ln -s `pwd`/perf /usr/bin/perf
else
	# DPDK
	cd $HOME
	sudo apt install -y meson ninja-build python3-pyelftools libpcap-dev
	git clone git://dpdk.org/dpdk
	cd dpdk
	git checkout v21.11
	meson build
	cd build
	ninja
	sudo ninja install
	# pktgen
	cd $HOME
	sudo apt install -y libnuma-dev
	git clone git://dpdk.org/apps/pktgen-dpdk
	cd pktgen-dpdk
	git checkout pktgen-21.11.0
	meson build
	cd build
	ninja
	sudo ninja install
	# igb_uio
	cd $HOME
	git clone git://dpdk.org/dpdk-kmods
	cd dpdk-kmods/linux/igb_uio/
	make
	sudo modprobe uio
	sudo insmod ./igb_uio.ko
	# Copy bind script
	cp /proj/progstack/farbod/bind.sh $HOME/
	# install moongen
	cd $HOME
	git clone https://github.com/emmericp/MoonGen
	sudo apt-get install -y build-essential cmake linux-headers-`uname -r` pciutils libnuma-dev
	# This is needed because for some reason builtin verion is not found by linker
	sudo apt install -y libtbb2
	cd MoonGen
	./build.sh
fi

echo "You should reboot the system (for hugepages/ new kernel)"
