#!/bin/tclsh

#Usage: workload.tcl <#files> <avgfilesize> <dir>
#Example: tclsh workload.tcl 10 1k /tmp/
# Creates 10 1kilo byte files in directory

proc mkfile {sz name} {
#exec head -c $size /dev/zero > $name
    set payload "sample file \n"
    while {$sz - [string length $payload] > 0} {
	append payload [format "%4d: 0123456789abcdef\n" [string length $payload]]
    }
    set fd [open $name w]
    puts -nonewline $fd $payload
    close $fd
}

set n [lindex $argv 0]
set size  [lindex $argv 1]
set dir   [lindex $argv 2]
set prefix fnull

puts "$n : $size : $dir "

for {set i 1} {$i <= $n} {incr i} {
    mkfile $size $dir/$i.$prefix
}