#
# Test script for the TCP convergence layer's receiver_connect option.
# Used to enable reception of bundles across a firewall / NAT router.
#

set num_nodes 2
set network loopback
route set type "static"
set topology none;  # Disable the automatic route config
source "test/main.tcl"


# For node 0 (the receiver), configure the special interface to enable
# bundles to be received.
if {$id == 0} {
    interface add tcp host://$hosts(1):5000 receiver_connect
}

# For node 1 (the sender), configure an opportunistic link and a route
# to point to the peer (who hasn't come knocking yet)
if {$id == 1} {
    link add link-0 host://$hosts(0) OPPORTUNISTIC tcp
    route add bundles://internet/host://$hosts(0)/* host://$hosts(0)
}

test set initscript {
    log "/test" INFO "route dump:\n[route dump]"
    
    if {$id == 0} {
	proc test_arrived {regid bundle_info} {
	    foreach {source dest payload length} $bundle_info {}
	    puts "bundle arrival"
	    puts "source:  $source"
	    puts "dest:    $dest"
	    puts "length:  $length"
	    puts "payload: [string range $payload 0 64]"
	}

	tcl_registration "bundles://*/*" test_arrived
    }
    
    if {$id == 1} {
	sendbundle
    }
}

