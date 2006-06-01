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

# the basic test group
set tests(basic) {
    "send-one-bundle.tcl"	"-cl tcp"
    "send-one-bundle.tcl"	"-cl tcp -length 0"
    "send-one-bundle.tcl"	"-cl udp"
    "send-one-bundle.tcl"	"-cl udp -length 0"
    "bundle-status-reports.tcl"	""
    "tcp-ondemand-links.tcl"	""
    "tcp-alwayson-links.tcl"	""
    "tcp-receiver-connect.tcl"	""
    "dtn-ping.tcl"		""
    "bidirectional.tcl"         ""
    "tcp-bogus-link.tcl"	""
    "tcp-writeblocked.tcl"	""
    "reactive-fragmentation.tcl"	""
    "many-bundles.tcl"		""
    "storage.tcl"		""
    "storage.tcl"		"-storage_type filesysdb"
    "api-leak-test.tcl"		""
    "multipath-forwarding.tcl"	""
    "multipath-forwarding.tcl"	"--linktype ONDEMAND"
    "loopback-link.tcl"		""
    "flood-router.tcl"		""
}

# the performance test group
set tests(perf) {
    "dtn-perf.tcl"		""
    "dtn-perf.tcl"		"-payload_len 50B"
    "dtn-perf.tcl"		"-payload_len 5MB  -file_payload"
    "dtn-perf.tcl"		"-payload_len 50MB -file_payload"
}

# check test group
set group [lindex $argv 0]
set common_options [lrange $argv 1 end]

if {![info exists tests($group)]} {
    puts "Error: test group \"$group\" not defined"
    exit -1
}

# print a "Table of Contents" first before running the tests
puts "***"
puts "*** The following tests will be run:"
puts "***"
foreach {test testopts} $tests($group) {
    puts "***   $test $testopts"
}
puts "***\n"

# run the tests
foreach {test testopts} $tests($group) {
    puts "***"
    puts "*** $test $testopts"
    puts "***"

    if {$testopts != ""} {
	set options "$common_options --opts \"$testopts\""
    } else {
	set options $common_options
    }
    
    if [catch {
	eval exec [file dirname $argv0]/run-dtn.tcl test/$test -d $options \
		< /dev/null >&@ stdout
    } err ] {
	puts "unexpected error: $err"
    }
}


