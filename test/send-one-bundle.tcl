test::name send-one-bundle
net::num_nodes 3

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

conf::add dtnd 0 {
    proc test_arrived {regid bundle_info} {
	foreach {source dest payload length} $bundle_info {}
	puts "bundle arrival"
	puts "source:  $source"
	puts "dest:    $dest"
	puts "length:  $length"
	puts "payload: [string range $payload 0 64]"
    }

    # XXX/demmer move this to a tell
    test set initscript {
	tcl_registration dtn://host-0/test test_arrived
    }
}

set last_node [expr [net::num_nodes] - 1]
conf::add dtnd $last_node "set last_node $last_node\n\n"
conf::add dtnd $last_node {
    # XXX/demmer this should move to a 'tell' command
    test set initscript {
	after 5000
	sendbundle dtn://host-$last_node/test dtn://host-0/test \
	    length 5555 receive_rcpt true forward_rcpt true deletion_rcpt true delivery_rcpt true
    }
}

test::script {
    for {set i 0} {$i < [net::num_nodes]} {incr i} {
	dtn::run_node $i
    }
}