#
# Test script for the TCP convergence layer's receiver_connect option.
# Used to enable reception of bundles across a firewall / NAT router.
#

set num_nodes 2
set network loopback
set topology linear
route set type "static"

# source the main test script harness
source "test/main.tcl"

if {$id == 0} {
    # For node 0 (the receiver), configure the special interface to
    # enable bundles to be received after making the connection
    interface add tcp host://$hosts(1):$ports(tcp,1) receiver_connect
    
} elseif {$id == 1} {
    # For node 1 (the sender), we just set up a normal interface
    # as well as an opportunistic link and a route
    # to point to the peer (who hasn't come knocking yet)
    setup_interface tcp
    
    link add link-0 host://host-0 OPPORTUNISTIC tcp
    route add bundles://internet/host://host-0/* link-0

} else {
    error "test can only be run for two nodes"
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

