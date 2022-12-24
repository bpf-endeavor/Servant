#! /bin/bash

COLOR_RED='\033[0;31m'
COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[0;33m'
COLOR_OFF='\033[0m' # No Color

log() {
	echo -e ${COLOR_GREEN} $@ ${COLOR_OFF}
}

err() {
	echo -e ${COLOR_RED} $@ ${COLOR_OFF}
}

sudo echo "hello!"
CURDIR=`dirname $0`
CURDIR=`realpath $CURDIR`
DDIR=`realpath $CURDIR/_deps`
mkdir -p $DDIR
if [ ! -d $DDIR ]; then
	err "Failed to create directory at $DDIR"
	err "Dependancies will be installed in this directory."
	exit 1
fi

sudo apt-get update
sudo apt-get install -y build-essential libz-dev libelf-dev pkg-config
printf "\n\n"

# Configure HUGEPAGES
restart_required=0
log "Would you like to update GRUB config to enable HUGE pages (y/[n])?"
read cmd
if [ "x$cmd" = "xy" ]; then
	log "Updating GRUB..."
	grub='GRUB_CMDLINE_LINUX_DEFAULT="default_hugepagesz=1G hugepagesz=1G hugepages=4"'
	echo $grub | sudo tee -a /etc/default/grub
	sudo update-grub
	log "Warning: The previous kernel parameters will be overwritten!"
	restart_required=1
fi

# ubpf
cd $DDIR
git clone https://github.com/fshahinfar1/uBPF.git
cd uBPF/vm
make
if [ $? -ne 0 ]; then
	err "Failed to build uBPF"
	exit 1
fi
sudo make install
log "- uBPF"
# libbpf
cd $DDIR
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
log "- libbpf"
# llvm13
cd $CURDIR/docs/
sudo bash ./llvm.sh
log "- LLVM 13"
# Servant
cd $CURDIR
git submodule update --init
cd $CURDIR/src
make
sudo make install
log "- Done"

if [ $restart_required -gt 0 ]; then
	echo "You should reboot the system (for hugepages)"
fi
