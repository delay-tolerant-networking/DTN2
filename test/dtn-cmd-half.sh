#!/bin/csh

## First See base-cmd.sh 
## This script is called by above.
## base-cmd.sh defines all the variables used in this script


echo "Inside DTN specific setting " >> $info

set mylogroot  =    $logroot/node-$id
set myexeroot  =    $dtn2root/daemon
set baseconfig = $dtn2testroot/base-dtn.conf


if (! -e $mylogroot) then
      mkdir  $mylogroot
endif

# Now generate daemon for the node

if (($id == 1) || ($id == $maxnodes) || ($perhop == 1)) then

    set file =  $mylogroot/node-$id.conf

    
    echo  set id $id > $file
    echo  set maxnodes $maxnodes >>  $file
    echo  set perhop $perhop >> $file
    echo  set logdir $mylogroot >> $file
    echo set dtn2testroot $dtn2testroot >> $file
    echo set localdir $txdir >> $file
    echo set ftplogfile $ftplogfile >> $file
    cat $baseconfig >> $file


    setenv LD_LIBRARY_PATH /usr/local/BerkeleyDB.4.2/lib
    setenv HOME /tmp/ 

    #-l $mylogroo
     set cmd = "$myexeroot/bundleDaemon -t   -c $file  -o $info.bd >>& $info "
     echo $cmd  >> $info

     ## Actually execute the command 
 	$myexeroot/bundleDaemon  -d -t  -c $file -o $info.bd -l debug >>& $info

endif
