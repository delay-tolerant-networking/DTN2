
# test parameter settings
set num_nodes 2
set network loopback
set topology linear

# source the main test script harness
source "test/main.tcl"

# Set up the listening interface and the forwarding topology
setup_interface tcp
setup_${topology}_topology ONDEMAND tcp

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

	tcl_registration "*:*" test_arrived
    }
    
    if {$id == 1} {
	sendbundle
    }
}

