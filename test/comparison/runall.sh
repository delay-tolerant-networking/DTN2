#!/bin/bash



function run_set() {
	bwkb="$bw""kb"
	./gen-emu.sh rabin $nodes $num $size $lossrate $bwkb "$types"  "$prots" $waittime 
	modexp -r -s -e DTN,rabin tmp/rabin.emu
	sleep $sleeptime 
	key=rabin-N$nodes-L$lossrate-M$num-S$size-B$bw
	dir=/proj/DTN/nsdi/logs/$key
	mkdir -p $dir
	for prot in $prots; do
		for type in $types; do
			if [ "$type" == "ph" ]; then
				typeind=1
			else
				typeind=0
			fi
			rm -rf $dir/$prot$typeind
			mv -f /proj/DTN/nsdi/logs/rabin/$prot$typeind $dir
		done
	done
} 


bw=100


prots="dtn"
types="ph e2e"
nodes=5
lossrate=0

waittime=350
sleeptime=1000
num=10
size=500
run_set;
num=50
size=100
run_set;
num=500
size=10
run_set;





lossrate=0.05

waittime=350
sleeptime=1000
num=10
size=500
run_set;
num=50
size=100
run_set;
num=500
size=10
run_set;



lossrate=0.1

waittime=350
sleeptime=1000
num=10
size=500
run_set;
num=50
size=100
run_set;
num=500
size=10
run_set;




