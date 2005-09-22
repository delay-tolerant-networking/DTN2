#!/usr/bin/tclsh

source "oasys/test_utils/import.tcl"
set import_path [list [pwd]/test_utils [pwd]/test/nets [pwd]/oasys/test_utils]

import "test-lib.tcl"

if {[llength $argv] < 1} {
    puts "run-test.tcl <test script> options..."
    puts ""
    puts "Required:"
    puts "    <test script> Test script"
    puts "    <net file>    Network testbed file"
    puts ""
    run::usage
    exit
}

proc dtn_exec_opt_fcn {id} {
    global net::host net::portbase net::extra test::testname
    
    set dtn(exec_file) dtnd
    set dtn(exec_opts) "-i $id -t -c $test::testname.conf"

    return [array get dtn]
}

# standard manifest
manifest::file daemon/dtnd dtnd
manifest::dir  bundles

set test_script [lindex $argv 0]
# default args to the run command (prepended to the args list so
# user-specified ones will override the defaults)
set defaults [list -net localhost]
set args [concat $defaults [lrange $argv 1 end]]
run::run $test_script $args "oasys/test_utils" dtn_exec_opt_fcn
