#!/usr/bin/tclsh
source "oasys/test_utils/test-lib.tcl"
source "oasys/test_utils/run.tcl"

if {[llength $argv] < 2} {
    puts "run-test.tcl <test script> <net file> options..."
    puts ""
    puts "Required:"
    puts "    <net file>    Network testbed file"
    puts "    <test script> Test script"
    puts ""
    usage
    exit
}

set test_script [lindex $argv 0]
set net_file    [lindex $argv 1]
set args [lrange $argv 2 end]

proc dtn_exec_opt_fcn {id} {
    global net::host net::portbase net::extra test::testname
    
    set dtn(exec_file) dtnd
    set dtn(exec_opts) "-i $id -t -c $test::testname.conf"

    return [array get dtn]
}

# standard manifest
manifest::file daemon/dtnd dtnd
manifest::dir  bundles

run $args $test_script $net_file [pwd] "oasys/test_utils" dtn_exec_opt_fcn
