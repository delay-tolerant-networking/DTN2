#!/bin/tclsh

# Usage: workload.tcl <#files> <file size in KB> <dir>
# Example: tclsh workload.tcl 10 1000 /tmp/
# Creates 10 1MB files in directory

proc mkfile {sz name} {
    global FACTOR
        
    set payload ""
    set j 0    
    while {$j < [expr $FACTOR / 10]} {
        append  payload [format "%09d\n" [ expr $j*10 ] ]
	    incr j
    }
    set fd [open $name w]
    set todo [expr $FACTOR * $sz ] 
    set i 0
    while {$i < $sz} {
	    #set  payload [format "%4d: 0123456789abcdef\n" [string length $payload]]
        puts -nonewline $fd $payload
        incr i
    }
    close $fd
}

set FACTOR 1000
set n [lindex $argv 0]
set size  [lindex $argv 1]
set dir   [lindex $argv 2]
set prefix fnull

puts "Create $n files of size  $size KB  in directory $dir "

for {set i 1} {$i <= $n} {incr i} {
    mkfile $size $dir/$i.$prefix
}
