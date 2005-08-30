#
# Testing utility to set up a tcl callback registration
#

# Callback function that is triggered whenever a bundle is put on the
# registration's delivery list. Loops until there are no more bundles
# on the list, calling the delivery callback on each one

proc tcl_registration_bundle_ready {regid endpoint callback callback_data} {
    while {1} {
	set bundle_data [registration tcl $regid $endpoint get_bundle_data]
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

proc tcl_registration {endpoint callback {callback_data ""}} {
    set regid [registration add tcl $endpoint]
    set chan [registration tcl $regid get_list_channel]
    fileevent $chan readable [list tcl_registration_bundle_ready \
	    $regid $endpoint $callback $callback_data]
    return $regid
}
