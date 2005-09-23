import "dtn-config.tcl"
import "dtn-topology.tcl"

# standard dtn manifest
manifest::file daemon/dtnd dtnd
manifest::file test_utils/dtnd-test-utils.tcl dtnd-test-utils.tcl
manifest::dir  bundles
manifest::dir  db

namespace eval dtn {
    proc run_node { id { exec_file "dtnd" } {other_opts ""} } {
	global opt net::host net::portbase net::extra test::testname
	
	if [string equal $exec_file "dtnd"] {
	    set exec_opts "-i $id -t -c $test::testname.conf"
	    if {! $opt(xterm)} {
		append exec_opts " -d"
	    }	    
	    # XXX/demmer add config hook for console (not just cmdline arg)
	    append dtnd(exec_opts) " --console-port [dtn_port console $id]"
	} else {
	    set exec_opts "$other_opts"
	}

	run::run $id $exec_file $exec_opts $test::testname.conf \
	    [conf::get dtnd $id]
    }
}