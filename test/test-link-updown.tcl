test::name test-link-updown
net::num_nodes 3

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true \
	connect_retry=2000 connect_timeout=4000 \
	min_retry_interval=5 max_retry_interval=10

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

    puts "* sending bundle"
    dtn::tell_dtnd 2 {
	sendbundle dtn://host-2 dtn://host-0/test
    }

    puts "* waiting for bundle arrival"
    do_until "checking for bundle arrival" 5000 {
	set stats [dtn::tell_dtnd 0 "bundle stats"]
	if {[string match "* 1 received *" $stats]} {
	    break
	}
    }

    puts "* checking that link is open"
    dtn::check_link_state 2 link-1 OPEN

    puts "* killing daemon 1"
    dtn::tell_dtnd 1 {exit}
    
    puts "* checking that link is unavailable"
    dtn::wait_for_link_state 2 link-1 UNAVAILABLE

    puts "* waiting for link to become available"
    dtn::wait_for_link_state 2 link-1 AVAILABLE

    puts "* sending another bundle"
    dtn::tell_dtnd 2 {
	sendbundle dtn://host-2 dtn://host-0/test
    }

    puts "* checking that link is now UNAVAILABLE"
    dtn::wait_for_link_state 2 link-1 UNAVAILABLE

    puts "* waiting for a few retry timers"
    after 30000
    
    puts "* checking that link is still UNAVAILABLE"
    dtn::wait_for_link_state 2 link-1 UNAVAILABLE
    
    puts "* restarting daemon 1"
    dtn::run_dtnd 1
    dtn::wait_for_dtnd 1
    
    puts "* checking that link is AVAILABLE"
    dtn::wait_for_link_state 2 link-1 UNAVAILABLE

    puts "* checking that bundle arrived"
    do_until "checking for bundle arrival" 5000 {
	set stats [dtn::tell_dtnd 0 "bundle stats"]
	if {[string match "* 2 received *" $stats]} {
	    break
	}
    }
    puts "* test success!"
}

test::exit_script {
    puts "* stopping all dtnds"
    dtn::tell_dtnd * exit
}
