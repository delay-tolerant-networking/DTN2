#!/bin/bash



function run_set() {
	bwkb="$bw""kb"
	./gen-emu.sh rabin $nodes $num $size $lossrate $bwkb "$types"  "$prots" $waittime 
	modexp -r -s -e DTN,rabin tmp/rabin.emu
	sleep $sleeptime 
	key=rabin-N$nodes-L$lossrate-M$num-S$size-B$bw
	dir=/proj/DTN/nsdi/logs/rabin/$key
	mkdir $dir
	for prot in $prots; do
		for type in $types; do
			if [ "$type" == "ph" ]; then
				typeind=1
			else
				typeind=0
			fi
			rm -rf $dir/$prot$typeind
			cp -rf /proj/DTN/nsdi/logs/rabin/$prot$typeind $dir
		done
	done
} 


bw=100


prots="tcp mail"
types="ph e2e"

lossrate=0

waittime=600
sleeptime=3000
num=10
size=500
nodes=5
run_set;



lossrate=0.05

waittime=600
sleeptime=3000
size=500
num=10
nodes=5
run_set;

waittime=600
sleeptime=3000
size=100
num=50
nodes=5
run_set;

waittime=600
sleeptime=3000
size=10
num=500
nodes=5
run_set;




prots="dtn"
types="ph e2e"
lossrate=0

waittime=600
sleeptime=3000
num=10
size=500
nodes=5
run_set;




