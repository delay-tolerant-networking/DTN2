#!/bin/csh

#Usage: mkfile.sh <size> <name>
#Creates empty file of given size. k for KB, m for MB. default byte.
#Example: mkfile 1m creates a file named 1m.file of size 1 Megabytes

if ($#argv != 2) then
    echo " Usage  mkfile.sh <size> <name> "
    exit 0
endif

echo "Creating file $2  of size $1"
head -c $1 /dev/zero > $2
