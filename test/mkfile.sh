#!/bin/sh

#Usage: mkfile <size>
#Creates empty file of given size. k for KB, m for MB. default byte.
#Example: mkfile 1m creates a file named 1m.file of size 1 Megabytes

suffix=file

echo "Creating file $1.$suffix  of size $1"
head -c $1 /dev/zero > $1.$suffix

