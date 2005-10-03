test::name send-one-bundle
net::num_nodes 3

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

conf::add dtnd 0 {
    proc test_arrived {regid bundle_info} {
	foreach {isadmin source dest length payload} $bundle_info {}
	if ($isadmin) {
	    error "Unexpected admin bundle arrival $source -> $dest"
	}
	puts "bundle arrival"
	puts "source:  $source"
	puts "dest:    $dest"
	puts "length:  $length"
	puts "payload: [string range $payload 0 64]"
    }
}

test::script {
    puts "* running dtnds"
    dtn::run_dtnd *

    puts "* waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    dtn::tell_dtnd 0 {
	tcl_registration dtn://host-0/test test_arrived
    }

    puts "* sending bundle"
    set last_node [expr [net::num_nodes] - 1]
    dtn::tell_dtnd $last_node {
	sendbundle [route local_eid] dtn://host-0/test
    }

    puts "* waiting for bundle arrival"
    do_until "checking for bundle arrival" 5000 {
	set stats [dtn::tell_dtnd 0 "bundle stats"]
	if {[string match "* 1 received *" $stats]} {
	    break
	}
    }

    puts "* test success!"
}

test::exit_script {
    puts "* stopping all dtnds"
    dtn::tell_dtnd * exit
}
