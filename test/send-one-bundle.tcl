
test::name send-one-bundle
net::num_nodes 3

dtn_config

dtn_config_interface tcp
dtn_config_linear_topology ONDEMAND tcp true

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

conf::add dtnd [expr [net::num_nodes] - 1] {
    # XXX/demmer this should move to a 'tell' command
    test set initscript {
	after 5000
	sendbundle dtn://host-1/test dtn://host-0/test
    }
}

