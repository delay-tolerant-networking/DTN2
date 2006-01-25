#!/usr/bin/tclsh
source "oasys/test-utils/import.tcl"
set import::path [list \
	[pwd]/test-utils \
	[pwd]/test/nets \
	[pwd]/oasys/test-utils \
	[pwd]/oasys/tclcmd \
	]

import "test-lib.tcl" 
import "dtn-test-lib.tcl"

if {[llength $argv] < 1} {
    puts "run-test.tcl <test script> options..."
    puts ""
    puts "Required:"
    puts "    <test script> Test script to run"
    puts ""
    run::usage
    real_exit
}

# default args
set defaults [list -net localhost]
set argv [concat $defaults $argv]
run::init $argv

# catch and report errors in the test script
if {[catch {test::run_script} err]} {
    global errorInfo
    puts "error in test [test::name]: $errorInfo"
}
if {!$opt(daemon)} {
    command_loop "dtntest"
}
if {[catch {test::run_exit_script} err]} {
    global errorInfo
    puts "error: $errorInfo"
}
run::wait_for_programs
run::collect_logs
run::cleanup
