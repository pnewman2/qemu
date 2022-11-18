#!/bin/sh

set -x

cd $(dirname $0)

resctrl=/sys/fs/resctrl

mbm_total_bytes()
{
	local total=0
	local group=$resctrl

	if [ -n $1 ]; then
		group=$resctrl/mon_groups/$1
	fi

	if [ ! -d $group ]; then
		echo "resctrl group directory not found: $group"
		exit 1
	fi

	mondata=$(cat $group/mon_data/mon_L3_*/mbm_total_bytes)
	for v in $mondata; do
		if [ "$v" = "Error" ]; then
			echo "Error"
		fi

		total=$((total + v))
	done

	echo $total
}

prepare_cpus()
{

	for cpudir in /sys/devices/system/cpu/cpu[0-9]*
	do
		cpud=$(basename $cpudir)
		cpuid=${cpud##cpu}
		taskset -c $cpuid ./count-chunk
	done

}

# Clear the Unavailable condition on each CPU.

prepare_cpus
t=$(mbm_total_bytes)
t=$(mbm_total_bytes)

if [ "$t" = "Error" ]; then
	echo "Unexpected mbm_total_bytes still returning 'Error'"
	exit 1
fi

mkdir $resctrl/mon_groups/m1
mkdir $resctrl/mon_groups/m2

echo $$ > $resctrl/mon_groups/m1/tasks
./count-chunk
./count-chunk

echo $$ > $resctrl/mon_groups/m2/tasks
./count-chunk
./count-chunk
./count-chunk
./count-chunk

if [ $(mbm_total_bytes m1) -ne 2 ]; then
	exit 2
fi

if [ $(mbm_total_bytes m2) -ne 4 ]; then
	exit 2
fi
