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
set dtn2root = $dtnroot/DTN2
set dtn2testroot = $dtn2root/test

# Root location to store log files

#set logroot = /proj/DTN/exp/$exp/logs/$proto
set logrootbase = $dtnroot/logs/$exp
set logroot     = $logrootbase/$proto
set rcvdir      = "$logroot/rcvfiles.$id"
set ftplogfile =  $logroot/ftplog.$id

set tcptimerfile = $dtn2testroot/tcp_retries2.sush

## stuff that u dont have to log in NFS
set localtmpdir       =  /tmp/log-$proto.$id
set txdir       = $localtmpdir/txfiles




#user etc
set username=`whoami`
set userhome=/users/$username

# Make the log directories

if (! -e $localtmpdir) then
    mkdir  $localtmpdir
endif


if (! -e $logrootbase) then
    mkdir  $logrootbase
endif


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


# Set TCP timers
#sudo "cp $tcptimerfile  /proc/sys/net/ipv4/tcp_retries2"
sudo sh -c "echo 3 >  /proc/sys/net/ipv4/tcp_retries2"
sudo sh -c "echo 2 >  /proc/sys/net/ipv4/tcp_syn_retries"


# If this is the source node generate workload
if ($id == 1) then  
   echo "Executing workload:  tclsh $dtn2root/test/workload.tcl $nfiles $size $txdir  " >> $info
   tclsh $dtn2root/test/workload.tcl $nfiles $size $txdir 
endif


set pingfile = $logroot/pinglog.$id
echo "#Check ping commands ...." > $pingfile 
set idplus  = `expr $id + 1`
#set linkname = link-$id
set nextnode=node-$idplus 
#sudo sh $dtn2testroot/check_ping.sh $nextnode >> $pingfile &


switch ($proto_orig)
	       case tcp:
                              source $dtn2testroot/tcp-cmd-half.sh
			      breaksw
               case dtn:
                              source $dtn2testroot/dtn-cmd-half.sh
                              breaksw
	       case mail:
                              source $dtn2testroot/mail/mail-cmd-half.sh
                              breaksw

               default:
                              echo "Not implemented protocol $proto_orig " >> $info
			      breaksw ;
endsw

