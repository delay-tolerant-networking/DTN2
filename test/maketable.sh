#!/bin/bash

usage="$0 <maxnodes> <expname>"
example="$0 3 tryfoo/ [basepath]"


if [ "$#" = 2 ]; then
    basepath=/proj/DTN/nsdi/logs
elif [ "$#" = 3 ]; then
    basepath=$3
else
    echo "Usage: $usage"
	echo "Example: $example"
    exit
fi

exp=$2
maxnodes=$1
    
#echo "Make table: exp:$exp  nodes:$maxnodes base=$basepath"

for proto in tcp0 tcp1 dtn0 dtn1 mail0 mail1 ; 
do
    exppath=$exp/$proto
    
    if [ ! -d $exppath ]; then
        continue
    fi

    ftplog=$basepath/$exppath/ftplog
    n=$maxnodes
    

    resfile=$basepath/$exppath/times.txt
    
    start=0
    end=-1

    txlen=0
    if [ -e $ftplog.$n ]; then 
        txlen=`cat $ftplog.$n | wc -l`
    fi

    rcvlen=0
    if [ -e $ftplog.1 ]; then 
        rcvlen=`cat $ftplog.1 | wc -l`
    fi

    if [ "$rcvlen" -gt "$txlen" ]; then
        rcvlen=$txlen
    fi


    if [ $rcvlen -eq 0 ]; then
        echo "Nothing to paste for $exp/$proto ftplog:$ftplog"
        continue
    fi
    echo -n "$exppath : Txlen:$txlen Rcvlen:$rcvlen ::"
    

    rm -f /tmp/$exp.$proto.index
    rm -f /tmp/$exp.$proto.$n
    rm -f /tmp/$exp.$proto.1
        
    lineno=1
    while [ "$lineno" -le "$rcvlen" ] 
    do
        echo "$lineno " >> /tmp/$exp.$proto.index
        let lineno=$lineno+1
    done
    cat $ftplog.1 | awk '{print $1}'  > /tmp/$exp.$proto.1
    cat $ftplog.$n | awk '{print $1}'  > /tmp/$exp.$proto.$n
    
    paste /tmp/$exp.$proto.index /tmp/$exp.$proto.1 /tmp/$exp.$proto.$n > $resfile


    #echo "ftp duration is: start of sending first file to receiving last file"
    #echo
    
    
    start=`cat $resfile | head -n 1| awk '{print $2}'`
    end=`cat $resfile | tail -n 1| awk '{print $3}'`
    
    let diff=$end-$start
    if [ "$rcvlen" -le "0" ]; then
	    diff=-1  
    fi
    echo  " Time:$diff "
done
