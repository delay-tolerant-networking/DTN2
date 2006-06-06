#!/usr/bin/tclsh

#
# Before the import, snarf out the base test dir option so we can
# properly set up the import path
#
set base_test_dir [pwd]
if {[llength $argv] >= 2} {
    for {set i 0} {$i < [llength $argv]} {incr i} {
	if {[lindex $argv $i] == "--base-test-dir"} {
	    set base_test_dir [file normalize [pwd]/[lindex $argv [expr $i + 1]]]
	}
    }
}

source "$base_test_dir/oasys/test-utils/import.tcl"
set import::path [list \
	$base_test_dir/test-utils \
	$base_test_dir/test/nets \
	$base_test_dir/oasys/test-utils \
	$base_test_dir/oasys/tclcmd \
	]

import "test-lib.tcl" 
import "dtn-test-lib.tcl"

if {[llength $argv] < 1} {
    puts "run-dtn.tcl <init_options> <test script> <options>..."
    puts ""
    puts "Required:"
    puts "    <test script> Test script to run"
    puts ""
    run::usage

    if {[info commands real_exit] != ""} {
	real_exit
    } else {
	exit
    }
}

# no buffering for stdout
fconfigure stdout -buffering none

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
    command_loop "dtntest% "
}
if {[catch {test::run_exit_script} err]} {
    global errorInfo
    puts "error: $errorInfo"
}
run::wait_for_programs
run::collect_logs
run::cleanup
if {[catch {test::run_cleanup_script} err]} {
    global errorInfo
    puts "error: $errorInfo"
}
