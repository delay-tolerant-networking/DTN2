#!/bin/sh

OTHERHOST=$1

    
COUNT=4
PACKET_SIZE=50;
INTERVAL=0.2
TIMEOUT=3
RETRY=5
rm -f /tmp/ping.$OTHERHOST


starttime=`date +%s`
ps -eo cmd > /tmp/$starttime
#CHeck if already running, if yes then exitcat
num=`grep check_ping /tmp/$starttime | wc -l` 
echo $num
if [ $num -gt 1 ]; then
    exit 0
fi

while [ 1 ]
do
    timenow=`date +%s`
    let incr=$timenow-$starttime
    ping -q -w $TIMEOUT -i $INTERVAL -c $COUNT -s $PACKET_SIZE $OTHERHOST 2>/dev/null >/dev/null 
    ret=$?
    if [ "$ret" == "0" ]; then
        echo $incr UP " At:$timenow :Retry after $RETRY seconds to $OTHERHOST"  
    else
        echo $incr DOWN " At:$timenow :Retry after $RETRY seconds to $OTHERHOST"
    fi
    sleep $RETRY 
done
        
