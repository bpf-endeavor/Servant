#! /bin/bash

moongen_base=$HOME/MoonGen
moongen=$moongen_base/build/MoonGen
config=$HOME/mytest.lua
result_dir=$HOME/latency_results/
duration=20

if [ ! -f $moongen ]; then
	echo Looks like moongen is not installed at $moongen
	exit 1
fi

if [ ! -f $config ]; then
	echo config not found
	exit 1
fi

if [ ! -d $result_dir ]; then
	mkdir -p $result_dir
fi

echo "Give a name to the experiment"
read expname
echo "How many times to repeat the experiment"
read repeat

experiment() {
	experiment_number=0
	while [ -d $result_dir/$expname/$experiment_number ]; do
		experiment_number=$((experiment_number + 1))
	done
	result_path=$result_dir/$expname/$experiment_number/
	mkdir -p $result_path

	sudo $moongen $config 0 &> $result_path/stdout.txt &
	echo moongen running
	sleep $duration

	sudo pkill MoonGen
	while [ -z `pidof MoonGen` ]; do
		sleep 1
		sudo pkill MoonGen
		echo !Resend the signal
	done
	sleep 1

	cp histogram.csv $result_path
	echo stored results at $result_path
	cat $result_path/stdout.txt
}

for i in `seq $repeat`; do
	echo "Attempt $i/$repeat"
	echo "---------------------------------"
	experiment
done
