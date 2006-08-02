test::name reactive-fragmentation
net::num_nodes 2

dtn::config
dtn::config_interface tcp
dtn::config_linear_topology ALWAYSON tcp true \
	"test_write_delay=1000 block_length=512"

test::script {
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    dtn::tell_dtnd 1 tcl_registration dtn://host-1/test

    puts "* Waiting for links to open"
    dtn::wait_for_link_state 0 tcp-link:0-1 OPEN
    dtn::wait_for_link_state 1 tcp-link:1-0 OPEN

    puts "* Sending bundle"
    set timestamp [dtn::tell_dtnd 0 sendbundle \
	    dtn://host-0/test dtn://host-1/test length=8096]

    for {set i 0} {$i < 5} {incr i} {
	set wait [expr 2000 + int(2500 * rand())]
	puts "* Waiting [expr $wait / 1000] seconds"
	after $wait
	
	puts "* Interrupting the link"
	dtn::tell_dtnd 0 link close tcp-link:0-1 
	dtn::wait_for_link_state 0 tcp-link:0-1 UNAVAILABLE
	dtn::tell_dtnd 0 link open tcp-link:0-1
	dtn::wait_for_link_state 0 tcp-link:0-1 OPEN
    }

    puts "* Waiting for the bundle to arrive and to be reassembled"
    dtn::wait_for_bundle 1 "dtn://host-0/test,$timestamp" 30000

    puts "* Checking that no bundles are pending"
    dtn::wait_for_bundle_stats 0 {0 pending}
    dtn::wait_for_bundle_stats 1 {0 pending 1 delivered}

    puts "* Checking that it really did fragment"
    set stats [dtn::tell_dtnd 1 bundle stats]
    regexp {(\d+) received} $stats match received
    if {$received == 1} {
	error "only one bundle received"
    }
    puts "* $received fragments received"

    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
