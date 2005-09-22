#!/usr/bin/tclsh

source "oasys/test_utils/import.tcl"
set import_path [list [pwd]/test_utils [pwd]/test/nets [pwd]/oasys/test_utils]
import "test-lib.tcl" ; # from oasys
import "dtn-test-lib.tcl"

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

# default args
set defaults [list -net localhost]
set test_script [lindex $argv 0]
set args [concat $defaults [lrange $argv 1 end]]

run::init $args $test_script

run::run [net::nodelist] dtnd_opts
