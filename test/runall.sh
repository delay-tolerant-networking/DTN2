#!/bin/bash



function run_set() {
	bwkb="$bw""kb"
	./gen-emu.sh rabin $nodes $num $size $lossrate $bwkb "$types"  "$prots" $waittime 
	modexp -r -s -e DTN,rabin tmp/rabin.emu
	sleep $sleeptime 
	key=rabin-N$nodes-L$lossrate-S$size-B$bw
	dir=/proj/DTN/logs/rabin/$key
	mkdir $key
	for prot in $prots; do
		for type in $types; do
			if [ "$type" == "ph" ]; then
				typeind=1
			else
				typeind=0
			fi
			echo "rm -rf $dir/$prot$typeind"
			echo "cp -rf /proj/DTN/logs/rabin/$prot$typeind $dir"
		done
	done
} 


bw=100
num=10
lossrate=0


sleeptime=1000
prots="mail"
types="ph"


waittime=1000
sleeptime=1500
size=500
nodes=4
run_set;
nodes=3
run_set;
nodes=2
run_set;

exit;

waittime=250
sleeptime=1600
size=100
nodes=5
run_set;
nodes=4
run_set;
nodes=3
run_set;
nodes=2
run_set;


size=10
nodes=5
run_set;
nodes=4
run_set;
nodes=3
run_set;
nodes=2
run_set;


