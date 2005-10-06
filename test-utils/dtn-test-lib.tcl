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
    
    proc tell_dtnd { id args } {
	global net::host
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		eval tell_dtnd $id $args
	    }
	    return
	}
	return [eval "tell::tell $net::host($id) [dtn::get_port console $id] $args"]
    }

    proc check_bundle_arrived {id bundle_guid} {
	return [dtn::tell_dtnd $id "info exists bundle_info($bundle_guid)"]
    }

    proc wait_for_bundle {id bundle_guid {timeout 30000}} {
	do_until "wait_for_bundle $bundle_guid" $timeout {
	    if {[check_bundle_arrived $id $bundle_guid]} {
		break
	    }
	}
    }

    proc get_bundle_data {id bundle_guid} {
	return [dtn::tell_dtnd $id "set bundle_info($bundle_guid)"]
    }

    proc check_bundle_data {id bundle_guid {args}} {
	array set bundle_data [get_bundle_data $id $bundle_guid]
	foreach {var val} $args {
	    if {$bundle_data($var) != $val} {
		error "check_bundle_data: bundle $bundle_guid \
			$var $bundle_data($var) != expected $val"
	    }
	}
    }

    proc check_bundle_stats {id args} {
        set stats [dtn::tell_dtnd $id "bundle stats"]
	foreach {val stat_type} $args {
	    if {![string match "* $val $stat_type *" $stats]} {
		error "node $i stat check for $stat_type failed \
		       expected $val but stats=\"$stats\""
	    }
	}
    }
    
    proc test_bundle_stats {id args} {
        set stats [dtn::tell_dtnd $id "bundle stats"]
	foreach {val stat_type} $args {
	    if {![string match "* $val $stat_type *" $stats]} {
		return false
	    }
	}
	return true
    }

    proc wait_for_stat {id val stat_type {timeout 30000}} {
	do_until "in wait for node $id's stat $stat_type = $val" $timeout {
	    if {[test_bundle_stats $id $val $stat_type]} {
		break
	    }
	}
    }

    # separate procedure because this one requires an explicit list
    # argument to allow for optional timeout argument
    proc wait_for_stats {id stat_list {timeout 30000}} {
	foreach {val stat_type} $stat_list {
	    do_until "in wait for node $id's stat $stat_type = $val" $timeout {
		if {[test_bundle_stats $id $val $stat_type]} {
		    break
		}
	    }
	}
    }

    proc check_link_state { id link state {log_error 0}} {
	global net::host
	
	set result [tell_dtnd $id "link state $link"]

	if {$result != $state} {
	    if {$log_error} {
		puts "ERROR: check_link_state: \
			id $id expected state $state, got $result"
	    }
	    return 0
	}
	return 1
    }

    proc wait_for_link_state { id link state {timeout 30000} } {
	do_until "waiting for link state $state" $timeout {
	    if [check_link_state $id $link $state 0] {
		return
	    }
	    after 1000
	}
    }
}
