############################################################
#
# dtnd-test-utils.tcl
#
# Test utility functions that are sourced by dtnd's config
# file when run through the testing infrastructure.
#
############################################################

#
# Callback function that is triggered whenever a bundle is put on the
# registration's delivery list. Loops until there are no more bundles
# on the list, calling the delivery callback on each one
#
proc tcl_registration_bundle_ready {regid endpoint callback callback_data} {
    while {1} {
	set bundle_data [registration tcl $regid get_bundle_data]
	if {$bundle_data == {}} {
	    return
	}

	if {$callback_data != ""} {
	    $callback $regid $bundle_data $callback_data
	} else {
	    $callback $regid $bundle_data
	}
    }
}

#
# Set up a new tcl callback registration
#
proc tcl_registration {endpoint {callback default_bundle_arrived} {callback_data ""}} {
    set regid [registration add tcl $endpoint]
    after 1000;     # XXX/demmer fix me
    set chan [registration tcl $regid get_list_channel]
    fileevent $chan readable [list tcl_registration_bundle_ready \
	    $regid $endpoint $callback $callback_data]
    return $regid
}

proc default_bundle_arrived {regid bundle_data} {
    array set b $bundle_data
    global bundle_payloads
    global bundle_info
    global bundle_sr_info
    
    puts "bundle arrival"
    foreach {key val} [array get b] {
	if {$key == "payload"} {
	    puts "payload:\t [string range $val 0 64]"
	} else {
	    puts "$key:\t $val"
	}
    }
    # record the bundle's arrival
    set guid "$b(source),$b(creation_ts)"
    set bundle_payloads($guid) $b(payload)
    unset b(payload)
    set bundle_info($guid) [array get b]
    if { $b(isadmin) && [string match "Status Report" $b(admin_type)] } {
	set sr_guid "$b(orig_source),$b(orig_creation_ts),$b(source)"
	set bundle_sr_info($sr_guid) [array get b]
    }	
}


#
# test proc for sending a bundle
#
proc sendbundle {source_eid dest_eid args} {
    global id

    # assume args consists of a list of valid "bundle inject" options
    set length 5000
    set i [lsearch -glob $args length=*]
    if {$i != -1} {
	set length [lindex $args [expr $i + 1]]
	set length [string map {length= ""} [lindex $args $i]]
    }
    set payload "test bundle payload data\n"

    while {$length - [string length $payload] > 32} {
	append payload [format "%4d: 0123456789abcdef\n" \
		[string length $payload]]
    }
    while {$length > [string length $payload]} {
	append payload "."
    }

    return [eval [concat {bundle inject $source_eid \
			      $dest_eid $payload length=$length} $args]]
}

