#!/bin/csh

# Usage: ./gentop.sh <expname> <maxnodes> <nfiles> <size> <proto> <perhop> <loss> <bw>
# perhop is 0 or 1, 0 if e2e , 1 if perhop

# Example: ./gentop.sh try 3 10 10000 tcp 0 0.1 1Mb

  if ($#argv != 8) then
        echo "Usage: $0 <expname> <maxnodes> <nfiles> <size> <proto> <perhop> <loss> <bw>"
       	echo "Example: $0 try 3 10 10000 tcp 0"
	echo " This will generate a file name <expname.emu> "
    exit
    endif

set exp = $1
set file = $exp.emu

echo "Automatic emulab topolgy generation. Output file $file" 



echo "#Automatic emulab topolgy generation" > $file

echo set exp $1 >> $file
echo set maxnodes $2 >> $file
echo set nfiles $3 >> $file
echo set size $4 >> $file
echo set proto $5 >> $file
echo set perhop $6 >> $file 
echo set loss $7 >> $file 
echo set bw $8 >> $file 
cat base.emulab.sh >> $file

echo "Now run: startexp -i -p DTN -e $exp $file"
