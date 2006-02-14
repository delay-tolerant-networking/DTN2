test::name tcp-writeblocked.tcl
net::num_nodes 2

dtn::config
dtn::config_interface tcp test_read_delay=500 readbuf_len=4096
dtn::config_linear_topology ALWAYSON tcp true \
	busy_queue_depth=2

test::script {
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    dtn::tell_dtnd 0 tcl_registration dtn://host-0/test
    dtn::tell_dtnd 1 tcl_registration dtn://host-1/test

    puts "* Waiting for links to open"
    dtn::wait_for_link_state 0 tcp-link:0-1 OPEN
    dtn::wait_for_link_state 1 tcp-link:1-0 OPEN

    set N 20
    puts "* Sending $N bundles in one direction"
    for {set i 0} {$i < $N} {incr i} {
	set timestamp($i) [dtn::tell_dtnd 1 sendbundle \
		dtn://host-1/test dtn://host-0/test length=4096]
    }

    for {set i 0} {$i < $N} {incr i} {
	puts "* Waiting for arrival of bundle $i"
	dtn::wait_for_bundle 0 "dtn://host-1/test,$timestamp($i)" 5000
    }
    
    puts "* Doing sanity check on stats"
    dtn::check_bundle_stats 0 0 pending $N received $N delivered
    dtn::check_bundle_stats 1 0 pending $N transmitted

    unset timestamp
    tell_dtnd * bundle reset_stats

    puts "* Sending $N bundles in both directions"
    for {set i 0} {$i < $N} {incr i} {
	set timestamp(0,$i) [dtn::tell_dtnd 0 sendbundle \
		dtn://host-0/test dtn://host-1/test length=4096]
	
	set timestamp(1,$i) [dtn::tell_dtnd 1 sendbundle \
		dtn://host-1/test dtn://host-0/test length=4096]
    }

    for {set i 0} {$i < $N} {incr i} {
	puts "* Waiting for arrival of bundle $i at each node"
	dtn::wait_for_bundle 0 "dtn://host-1/test,$timestamp(1,$i)" 5000
	dtn::wait_for_bundle 1 "dtn://host-0/test,$timestamp(0,$i)" 5000
    }
    
    puts "* Doing sanity check on stats"
    dtn::check_bundle_stats 0 0 pending [expr $N * 2] received $N delivered
    dtn::check_bundle_stats 1 0 pending [expr $N * 2] received $N delivered
    dtn::check_bundle_stats 0 0 pending $N transmitted
    dtn::check_bundle_stats 1 0 pending $N transmitted

    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
