import "dtn-config.tcl"
import "dtn-topology.tcl"

# procedure naming conventions:
#
# test_*     - returns true/false for a given check
# check_*    - errors out if the check fails
# wait_for_* - errors out if the check never succeeds within a timeout

proc tell_dtnd {id args} {
    return [eval dtn::tell_dtnd $id $args]
}

proc tell_dtntest {id args} {
    return [eval dtn::tell_dtntest $id $args]
}

namespace eval dtn {
    proc run_dtnd { id {other_opts "-t"} } {
	global opt net::host net::portbase net::extra test::testname
	
	if {$id == "*"} {
	    set pids ""
	    foreach id [net::nodelist] {
		lappend pids [run_dtnd $id $other_opts]
	    }
	    return $pids
	}
	
	set exec_opts "-i $id -c $test::testname.conf"

	append exec_opts " $other_opts"

	return [run::run $id "dtnd" $exec_opts $test::testname.conf \
		    [conf::get dtnd $id] ""]
	
    }

    proc run_dtntest { id {other_opts ""}} {
	global opt net::host net::portbase net::extra test::testname
	
	if {$id == "*"} {
	    set pids ""
	    foreach id [net::nodelist] {
		lappend pids [run_dtntest $id $other_opts]
	    }
	    return $pids
	}

	set confname "$test::testname.dtntest.conf"
	set exec_opts "-c $confname"
	append exec_opts " $other_opts"

	set addr [gethostbyname $net::host($id)]
	
	lappend exec_env DTNAPI_ADDR $addr
	lappend exec_env DTNAPI_PORT [dtn::get_port api $id]

	return [run::run $id "dtntest" $exec_opts \
		$confname [conf::get dtntest $id] $exec_env]
	
    }

    proc stop_dtnd {id} {
	global net::host
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		stop_dtnd $id
	    }
	    return
	}

	if [catch {
	    tell_dtnd $id shutdown
	    after 500
	} err] {
	    puts "ERROR: error in shutdown of dtnd id $id"
	}
	catch {
	    tell::close_socket $net::host($id) [dtn::get_port console $id]
	}
    }
    
    proc stop_dtntest {id} {
	global net::host
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		stop_dtntest $id
	    }
	    return
	}

	if [catch {
	    tell_dtntest $id shutdown
	} err] {
	    puts "ERROR: error in shutdown of dtnd id $id"
	}
	catch {
	    tell::close_socket $net::host($id) [dtn::get_port dtntest $id]
	}
    }

    proc run_app { id app_name {exec_args ""} } {
	global opt net::host net::portbase net::extra test::testname
	
	if {$id == "*"} {
	    set pids ""
	    foreach id [net::nodelist] {
		lappend pids [run_app $id $app_name $exec_args]
	    }
	    return $pids
	}

	set addr [gethostbyname $net::host($id)]
	
	lappend exec_env DTNAPI_ADDR $addr
	lappend exec_env DTNAPI_PORT [dtn::get_port api $id]
	
	return [run::run $id "$app_name" $exec_args \
		    $test::testname-$app_name.conf \
		    [conf::get $app_name $id] $exec_env]
	
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
    
    proc wait_for_dtntest {id} {
	global net::host
	
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		wait_for_dtnd $id
	    }
	}

	tell::wait $net::host($id) [dtn::get_port dtntest $id]
    }
    
    proc tell_dtnd { id args } {
	global net::host
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		eval tell_dtnd $id $args
	    }
	    return
	}
	return [eval "tell::tell $net::host($id) \
		[dtn::get_port console $id] $args"]
    }

    proc tell_dtntest { id args } {
	global net::host
	if {$id == "*"} {
	    foreach id [net::nodelist] {
		eval tell_dtntest $id $args
	    }
	    return
	}
	return [eval "tell::tell $net::host($id) \
		[dtn::get_port dtntest $id] $args"]
    }

    # generic checker function
    proc check {args} {
	set orig_args $args
	
	set expected 1
	if {[lindex $args 0] == "!"} {
	    set expected 0
	    set args [lrange $args 1 end]
	}

	set result [eval $args]
	if {$result != $expected} {
	    error "check '$orig_args' failed"
	}
    }
    
    # dtn bundle data functions

    proc check_bundle_arrived {id bundle_guid} {
	if {![dtn::tell_dtnd $id "info exists bundle_info($bundle_guid)"]} {
	    error "check for bundle arrival failed: \
	        node $id bundle $bundle_guid"
	}
    }

    proc wait_for_bundle {id bundle_guid {timeout 30000}} {
	do_until "wait_for_bundle $bundle_guid" $timeout {
	    if {![catch {check_bundle_arrived $id $bundle_guid}]} {
		break
	    }
	}
    }

    # this gets bundle data either through the tcl registration (in
    # which case it's in a global array bundle_info and bundle_guid is
    # a source,timestamp pair, or by calling the bundle dump_tcl
    # command if it's in the pending list, in in which case bundle_guid
    # should be of the form "bundleid-XXX"
    
    proc get_bundle_data {id bundle_guid} {
	if [regexp -- {bundleid-([0-9]+)} $bundle_guid match bundle_id] {
	    return [dtn::tell_dtnd $id "bundle dump_tcl $bundle_id"]
	} else {
	    return [dtn::tell_dtnd $id "set bundle_info($bundle_guid)"]
	}
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

    # dtn status report bundle data functions

    proc check_sr_arrived {id sr_guid} {
	if {![dtn::tell_dtnd $id "info exists bundle_sr_info($sr_guid)"]} {
	    error "check for SR arrival failed: node $id SR $sr_guid"
	}
    }

    proc wait_for_sr {id sr_guid {timeout 30000}} {
	do_until "wait_for_sr $sr_guid" $timeout {
	    if {![catch {check_sr_arrived $id $sr_guid}]} {
		break
	    }
	}
    }

    proc get_sr_data {id sr_guid} {
	return [dtn::tell_dtnd $id "set bundle_sr_info($sr_guid)"]
    }

    proc check_sr_data {id sr_guid {args}} {
	array set sr_data [get_sr_data $id $sr_guid]
	foreach {var val} $args {
	    if {$sr_data($var) != $val} {
		error "check_sr_data: SR $sr_guid \
			$var $sr_data($var) != expected $val"
	    }
	}
    }

    proc check_sr_fields {id sr_guid {args}} {
	array set sr_data [get_sr_data $id $sr_guid]
	foreach field $args {
	    if {![info exists sr_data($field)]} {
		error "check_sr_fields: SR \"$sr_guid\" field $field not found"
	    }
	}
    }

    # registration functions
    proc test_reg_exists {id regid} {
	if [catch {
	    tell_dtnd $id registration dump_tcl $regid
	} err] {
	    return 0
	}
	return 1
    }
    
    proc check_reg_data {id regid {args}} {
	array set reg_data [tell_dtnd $id registration dump_tcl $regid]
	foreach {var val} $args {
	    if {$reg_data($var) != $val} {
		error "check_reg_data: registration $regid \
			$var $reg_data($var) != expected $val"
	    }
	}
    }

    # dtnd "bundle stats" functions
    
    proc check_bundle_stats {id args} {
        set stats [dtn::tell_dtnd $id "bundle stats"]
	foreach {val stat_type} $args {
	    if {![string match "*$val ${stat_type}*" $stats]} {
		error "node $id stat check for $stat_type failed \
		       expected $val but stats=\"$stats\""
	    }
	}
    }
    
    proc test_bundle_stats {id args} {
        set stats [dtn::tell_dtnd $id "bundle stats"]
	foreach {val stat_type} $args {
	    if {![string match "*$val ${stat_type}*" $stats]} {
		return false
	    }
	}
	return true
    }

    proc wait_for_bundle_stat {id val stat_type {timeout 30000}} {
	do_until "in wait for node $id's stat $stat_type = $val" $timeout {
	    if {[test_bundle_stats $id $val $stat_type]} {
		break
	    }
	}
    }

    # separate procedure because this one requires an explicit list
    # argument to allow for optional timeout argument
    proc wait_for_bundle_stats {id stat_list {timeout 30000}} {
	foreach {val stat_type} $stat_list {
	    do_until "in wait for node $id's stat $stat_type = $val" $timeout {
		if {[test_bundle_stats $id $val $stat_type]} {
		    break
		}
	    }
	}
    }

    # dtnd "link state" functions

    proc check_link_state { id link state } {
	global net::host
	
	set result [tell_dtnd $id "link state $link"]

	if {$result != $state} {
	    error "ERROR: check_link_state: \
		id $id expected state $state, got $result"
	}
    }

    proc wait_for_link_state { id link states {timeout 30000} } {
	do_until "waiting for link state $states" $timeout {
	    foreach state $states {
		if {![catch {check_link_state $id $link $state}]} {
		    return
		}
	    }
	    after 1000
	}
    }

    # dtnd "link stats" functions
    
    proc check_link_stats {id link args} {
        set stats [dtn::tell_dtnd $id "link stats $link"]
	foreach {val stat_type} $args {
	    if {![string match "*$val ${stat_type}*" $stats]} {
		error "node $id link $link stat check for $stat_type failed \
		       expected $val but stats=\"$stats\""
	    }
	}
    }
    
    proc test_link_stats {id link args} {
        set stats [dtn::tell_dtnd $id "link stats $link"]
	foreach {val stat_type} $args {
	    if {![string match "*$val ${stat_type}*" $stats]} {
		return false
	    }
	}
	return true
    }

    proc wait_for_link_stat {id link val stat_type {timeout 30000}} {
	do_until "wait for node $id's link $link stat $stat_type = $val" \
		$timeout {
	    if {[test_bundle_stats $id $link $val $stat_type]} {
		break
	    }
	}
    }

    # separate procedure because this one requires an explicit list
    # argument to allow for optional timeout argument
    proc wait_for_link_stats {id link stat_list {timeout 30000}} {
	foreach {val stat_type} $stat_list {
	    do_until "wait for node $id's link $link stat $stat_type = $val" \
		    $timeout {
		if {[test_bundle_stats $id $link $val $stat_type]} {
		    break
		}
	    }
	}
    }

}
