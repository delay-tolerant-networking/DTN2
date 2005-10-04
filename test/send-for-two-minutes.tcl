test::name send-for-two-minutes
net::num_nodes 3

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

conf::add dtnd 0 {
    proc test_arrived {regid bundle_info} {
	array set b $bundle_info
	if ($b(isadmin)) {
	    error "Unexpected admin bundle arrival $b(source) -> b($dest)"
	}
	puts "bundle arrival"
	foreach {key val} [array get b] {
	    if {$key == "payload"} {
		puts "payload:\t [string range $b(payload) 0 64]"
	    } else {
		puts "$key:\t $b($key)"
	    }
	}
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

    set last_node [expr [net::num_nodes] - 1]
    
    puts "* sending one bundle every 10 seconds for 2 minutes"
    for {set i 1} {$i <= 13} {incr i} {
	puts "* sending  bundle $i"
	dtn::tell_dtnd $last_node {
	    sendbundle [route local_eid] dtn://host-0/test
	}
	do_until "checking for bundle $i arrival" 5000 [subst -nocommand {
	    set stats [dtn::tell_dtnd 0 "bundle stats"]
	    if {[string match "* $i received *" \$stats]} {
		puts "* received bundle $i"
		break
	    }
	} ]
	# save 10 seconds at the end of the test:
	if {$i < 13} {
	    after 10000
	}
    }

    puts "* test success!"
}

test::exit_script {
    puts "* stopping all dtnds"
    dtn::tell_dtnd * exit
}
