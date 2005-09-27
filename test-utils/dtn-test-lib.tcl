import "dtn-config.tcl"
import "dtn-topology.tcl"

namespace eval dtn {
    proc run_dtnd { id {other_opts ""} } {
	global opt net::host net::portbase net::extra test::testname
	
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		run_dtnd $id $other_opts
	    }
	    return
	}
	
	set exec_opts "-i $id -t -c $test::testname.conf"
	if {! $opt(xterm)} {
	    append exec_opts " -d"
	}	    

	# XXX/demmer add config hook for console (not just cmdline arg)
	append exec_opts " --console-port [dtn::get_port console $id]"

	append exec_opts " $other_opts"

	run::run $id "dtnd" $exec_opts $test::testname.conf \
		[conf::get dtnd $id]
    }

    proc wait_for_dtnd {id} {
	global net::host
	
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		wait_for_dtnd $id
	    }
	}

	tell::wait $net::host($id) [dtn::get_port console $id]
    }
    
    proc tell_dtnd { id cmd } {
	global net::host
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		tell_dtnd $id $cmd
	    }
	    return
	}
	return [tell::tell $net::host($id) [dtn::get_port console $id] $cmd]
    }
}
