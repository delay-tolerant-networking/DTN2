#!/bin/sh

F=/tmp/delivery_count

if [ ! -f $F ] ; then
    echo "1" > $F
else
    i=`cat $F`
    i=$(($i+1))
    echo $i > $F
fi
