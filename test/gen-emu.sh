#!/bin/csh

  if ($#argv != 9) then
        echo "Usage: $0 <expname> <maxnodes> <nfiles> <size(KB)> <loss> <bw> <perhop> <proto1, proto2,..> <finishtime>"
  	echo "Example: $0 try 4 10 100 0 100kb  [e2e ph] [tcp dtn mail] 711 "
	echo "Perhop can be 0,1,2: 0 e2e, 1 is perhop, 2 is both"
	echo " This will generate <expname.emu> and will run tcp, dtn"
	echo
    exit
    endif


set exp = $1
set file = tmp/$exp.emu

echo "Automatic emulab topolgy generation. Output file $file" 



echo "#Automatic emulab topolgy generation" > $file
echo "#Generated using command $* " > $file
echo set exp $1 >> $file
echo set maxnodes $2 >> $file
echo set nfiles $3 >> $file
echo set size $4 >> $file
echo set loss $5 >> $file 
echo set bw $6 >> $file 
echo set perhops \"$7\" >> $file 
echo set protos \"$8\" >> $file
echo set finish $9 >> $file
cat base-emu.tcl >> $file ;

echo 

echo "Now run: (make sure the directory tmp exists) "
echo "   startexp -i -p DTN -e $exp $file"
echo 
echo "To terminate "
echo "endexp -e DTN,$exp "
echo 
