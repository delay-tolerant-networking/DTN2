#!/bin/csh
## Usage: ./dtn-cmd.sh <nodeid> <exp> <maxnodes> <nfiles> <size> <perhop> 


if ($#argv != 6) then
        echo "Usage: Sorry nothing. see fileitself"
    exit
    endif

#echo Script: $0 
set  id =  $1
set  exp     = $2    
set  maxnodes = $3    
set nfiles     = $4
set size      = $5   
set  perhop  = $6    


set proto  =  dtn$perhop



## place to find DTN2. This will link to exe files
set dtnroot = /proj/DTN/nsdi/
set dtn2root = /proj/DTN/nsdi/DTN2img

# Root location to store log files
set logroot = /proj/DTN/exp/$exp/logs/$proto
set txdir = "$logroot/txfiles"
set rcvdir = "$logroot/rcvfiles"





# Make the log directory

if (! -e $logroot) then
    mkdir  $logroot
endif

if (! -e $txdir) then
mkdir  $txdir
endif

if (! -e $rcvdir) then
mkdir  $rcvdir
endif

set info = $logroot/info.$id
echo "Logging commands executed by $proto (script)" > $info

# If this is the source node generate workload
if ($id == 1) then  
   
   echo " executing  tclsh $dtn2root/test/workload.tcl $nfiles $size $txdir  " >> $info
   tclsh $dtn2root/test/workload.tcl $nfiles $size $txdir 
   

endif

# Now generate daemon for the node
if ($id == 1) then  
    set dst = node-$maxnodes
    if ($perhop == 1) then  
	set dst =  node-2-link-1
    endif
    
    echo " executing tclsh $dtn2root/test/simple-ftp.tcl client $txdir  $logroot/log.$id $dst  " >> $info
    tclsh $dtn2root/test/simple-ftp.tcl client $txdir  $logroot/log.$id $dst 
    
    ## mkdir logroot/id ???
    ## $dtn2root/daemon/bundleDaemon -t -id <id> -maxnodes <maxnodes> -l $logroot 
    exit

endif

if ($id == $maxnodes) then 
    echo " executing server with parameters $rcvdir " >> o
    tclsh $dtn2root/test/simple-ftp.tcl server $rcvdir  $logroot/log.$id 
    exit
endif     

## For other nodes which are in between
if ($perhop == 1) then
    ## set up TCP forwarding proxies
    set idplus  = `expr $id - 1`
    set idminus = `expr $id + 1`

    set myip = node-$id-link-$idminus
    set dstip = node-$idplus-link-$id
    set rconf = "$logroot/$id.rinetd-conf"
    echo $myip 17600 $dstip 17600 > $rconf 
    echo " executing rinetd at router ($myip, $dstip) " >> $info
    $dtnroot/rinetd/rinetd -c $rconf ;


endif
 
