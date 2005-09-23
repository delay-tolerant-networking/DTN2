#!/usr/bin/tclsh
source "oasys/test_utils/import.tcl"
set import::path [list [pwd]/test_utils [pwd]/test/nets [pwd]/oasys/test_utils]

import "test-lib.tcl" 
import "dtn-test-lib.tcl"

if {[llength $argv] < 1} {
    puts "run-test.tcl <test script> options..."
    puts ""
    puts "Required:"
    puts "    <test script> Test script to run"
    puts ""
    run::usage
    exit
}

# default args
set defaults [list -net localhost]
if { [llength $argv] > 0 } {
    set test_script [lindex $argv 0]
} else {
    set test_script "invalid"
}
set args [concat $defaults [lrange $argv 1 end]]

run::init $args $test_script

# ignore errors in the run_script, but leave crap if there are
eval test::run_script
run::wait_for_programs
run::collect_logs
run::cleanup
