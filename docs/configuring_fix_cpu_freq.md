# Fix CPU Frequency Settings

* sudo apt install libpci-dev
* Got to linux/tools/power/cpupower
* make
* sudo make install
* sudo ldconfig
* modprobe `cpufreq_userspace`
* cpupower frequency-set --governor userspace
* cpupower --cpu all frequency-set --freq 3.6GHz
* cat /proc/cpuinfo | grep MHz
