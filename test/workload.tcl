#!/bin/tclsh

# Usage: workload.tcl <#files> <file size in KB> <dir>
# Example: tclsh workload.tcl 10 1000 /tmp/
# Creates 10 1MB files in directory

proc mkfile {sz name} {
    global FACTOR

    set fd [open $name w]
    set todo [expr $FACTOR * $sz ] 
    set payload ""
    set i 0
    while {$todo > 0} {
	#set  payload [format "%4d: 0123456789abcdef\n" [string length $payload]]
	set  payload [format "%09d\n" [ expr $i*10 ] ]
	incr i
	puts -nonewline $fd $payload
	set todo [expr $todo - [string length $payload]]
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
