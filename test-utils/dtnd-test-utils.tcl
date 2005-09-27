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
proc tcl_registration {endpoint callback {callback_data ""}} {
    set regid [registration add tcl $endpoint]
    after 1000;     # XXX/demmer fix me
    set chan [registration tcl $regid get_list_channel]
    fileevent $chan readable [list tcl_registration_bundle_ready \
	    $regid $endpoint $callback $callback_data]
    return $regid
}

#
# test proc for sending a bundle
#
proc sendbundle {source_eid dest_eid args} {
    global id

    # XXX/matt for now just assume args consists of a list of "bundle
    # inject" option pairs
    set length  5000
    set i [lsearch -exact $args length]
    if {$i != -1} {
	set length [lindex $args [expr $i + 1]]
    }
    set payload "test bundle payload data\n"

    while {$length - [string length $payload] > 32} {
	append payload [format "%4d: 0123456789abcdef\n" \
		[string length $payload]]
    }
    while {$length > [string length $payload]} {
	append payload "."
    }

    set options [concat [list length $length] $args]
    bundle inject $source_eid $dest_eid $payload \
	option $options
}

