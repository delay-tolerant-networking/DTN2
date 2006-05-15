test::name reactive-fragmentation
net::num_nodes 2

dtn::config
dtn::config_interface tcp
dtn::config_linear_topology ALWAYSON tcp true \
	test_write_delay=1000 writebuf_len=512

test::script {
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    dtn::tell_dtnd 0 tcl_registration dtn://host-0/test

    puts "* Waiting for links to open"
    dtn::wait_for_link_state 0 tcp-link:0-1 OPEN
    dtn::wait_for_link_state 1 tcp-link:1-0 OPEN

    puts "* Sending bundle"
    set timestamp [dtn::tell_dtnd 1 sendbundle \
	    dtn://host-1/test dtn://host-0/test length=8096]

    for {set i 0} {$i < 5} {incr i} {
	set wait [expr int(5000 * rand())]
	puts "* Waiting [expr $wait / 1000] seconds"
	after $wait
	
	puts "* Interrupting the link"
	dtn::tell_dtnd 1 link close tcp-link:1-0 
	dtn::wait_for_link_state 1 tcp-link:1-0 UNAVAILABLE
	dtn::tell_dtnd 1 link open tcp-link:1-0
	dtn::wait_for_link_state 1 tcp-link:1-0 OPEN
    }
	
    puts "* Waiting for the bundle to arrive and to be reassembled"
    dtn::wait_for_bundle 0 "dtn://host-1/test,$timestamp" 30000

    puts "* Checking that no bundles are pending"
    dtn::wait_for_bundle_stats 0 {0 pending 1 delivered}
    dtn::wait_for_bundle_stats 1 {0 pending}
    
    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
