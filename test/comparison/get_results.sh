#!/bin/csh



set protlist="dtn mail tcp"




set lossrate=0
set bandwidth=100
set size=10

set outfile=$1
rm -f $outfile

foreach nodes(2 3 4 5)
	set startline="#nodes"
	set line=$nodes
	foreach size(10 100)
		foreach name($protlist)
			foreach type(0 1)
				set startline="$startline S$size.$name.$type"
				set exp="rabin-N$nodes-L$lossrate-S$size-B$bandwidth"
				set ftime=`/proj/DTN/nsdi/bin//ftptime.sh $nodes $exp/$name$type | tail -n 1`
				set line="$line $ftime"
			end
		end
	end
	echo $line
	echo $line >> $outfile
end
echo $startline >> $outfile

