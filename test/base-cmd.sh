#!/bin/csh

set usage =  "./base-cmd <nodeid> <exp-name> <maxnodes> <nfiles> <size> <perhop> <proto>"
set example = "Example: ./base-cmd.sh   1 try   4        10      10   1      tcp"

if ($#argv != 7) then
	echo "Runs a particular protocol at a certain node id "
	echo 
        echo "Usage: $usage"
	echo "Example: $example"
	echo
	
    exit 1
    endif

#echo Script: $0 
set  id =  $1
set  exp     = $2    
set  maxnodes = $3    
set nfiles     = $4
set size      = $5   
set  perhop  = $6    
set  proto_orig  = $7

set proto  = $proto_orig$perhop




## place to find DTN2. This will link to exe files
set dtnroot = /proj/DTN/nsdi
set dtn2root = /proj/DTN/nsdi/DTN2img
set dtn2testroot = /proj/DTN/nsdi/DTN2img/test

# Root location to store log files

#set logroot = /proj/DTN/exp/$exp/logs/$proto
set logrootbase = /proj/DTN/nsdi/logs/$exp
set logroot     = $logrootbase/$proto
set txdir       = "$logroot/txfiles"
set rcvdir      = "$logroot/rcvfiles"





# Make the log directories


if (! -e $logrootbase) then
    mkdir  $logrootbase
endif

if (! -e $logroot) then
    mkdir  $logroot
endif

if (! -e $txdir) then
mkdir  $txdir
endif

if (! -e $rcvdir) then
mkdir  $rcvdir
endif

set info = $logroot/log.$id
echo "Logging commands executed by $proto (node id $id)" > $info


# If this is the source node generate workload
if ($id == 1) then  
   echo "Executing workload:  tclsh $dtn2root/test/workload.tcl $nfiles $size $txdir  " >> $info
   tclsh $dtn2root/test/workload.tcl $nfiles $size $txdir 
endif



switch ($proto_orig)
	       case tcp:
                              source $dtn2testroot/tcp-cmd-half.sh
			      breaksw
               case dtn:
                              source $dtn2testroot/dtn-cmd-half.sh
                              breaksw
	       case mail:
                              source $dtn2testroot/mail-cmd-half.sh
                              breaksw

               default:
                              echo "Not implemented protocol $proto_orig " >> $info
			      breaksw ;
endsw

