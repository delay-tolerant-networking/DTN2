#!/bin/csh
set date=`date +%s`
set hostname=`hostname`
echo "[$date] :: got file $file at $hostname" >>& /users/rabin/ftplogfile
