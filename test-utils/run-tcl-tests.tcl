#!/usr/bin/tclsh

# usage: test-utils/run-tcl-tests.tcl <test group> <run-dtn.tcl options?>
if {[llength $argv] < 1} {
    puts "test-utils/run-tcl-tests.tcl <test group> options..."
    puts ""
    puts "Required:"
    puts "    <test group> Test group to run"
    puts ""
    exit
}

# associative array keyed by test group
set array tests

# only one group for now:
set tests(basic) {"send-one-bundle.tcl"
		  "send-for-two-minutes.tcl"
		  "bundle-status-reports.tcl"
		  "test-link-updown.tcl"
		  "tcp-receiver-connect.tcl"
		  "dtn-ping.tcl"
		  }


# check test group

set group [lindex $argv 0]
set options [lrange $argv 1 end]

if {![info exists tests($group)]} {
    puts "Error: test group \"$group\" not defined"
    exit -1
}

foreach test $tests($group) {
    puts "***"
    puts "*** $test"
    puts "***"
    if [catch {
	eval exec ./test-utils/run-dtn.tcl test/$test -d $options < /dev/null >&@ stdout
    } err ] {
	puts "unexpected error: $err"
    }
}


