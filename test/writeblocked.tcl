test::name writeblocked.tcl
net::num_nodes 2

set cl tcp

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set cl $val
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

dtn::config
dtn::config_interface $cl 
dtn::config_linear_topology ALWAYSON $cl true \
	"test_read_delay=500 block_length=1000"

test::script {
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    dtn::tell_dtnd 0 tcl_registration dtn://host-0/test
    dtn::tell_dtnd 1 tcl_registration dtn://host-1/test

    puts "* Waiting for links to open"
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN
    dtn::wait_for_link_state 1 $cl-link:1-0 OPEN

    puts "* Shrinking send/recv buffers"
    dtn::tell_dtnd 0 link reconfigure $cl-link:0-1 \
	    sendbuf_len=1000 recvbuf_len=1000
    
    dtn::tell_dtnd 1 link reconfigure $cl-link:1-0 \
	    sendbuf_len=1000 recvbuf_len=1000

    set N 20
    puts "* Sending $N bundles in one direction"
    for {set i 0} {$i < $N} {incr i} {
	set timestamp($i) [dtn::tell_dtnd 1 sendbundle \
		dtn://host-1/test dtn://host-0/test length=5000]
    }

    for {set i 0} {$i < $N} {incr i} {
	puts "* Waiting for arrival of bundle $i"
	dtn::wait_for_bundle 0 "dtn://host-1/test,$timestamp($i)" 10000
    }
    
    puts "* Doing sanity check on stats"
    dtn::wait_for_bundle_stats 0 "0 pending $N received $N delivered"
    dtn::wait_for_bundle_stats 1 "0 pending $N transmitted"

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
	dtn::wait_for_bundle 0 "dtn://host-1/test,$timestamp(1,$i)" 10000
	dtn::wait_for_bundle 1 "dtn://host-0/test,$timestamp(0,$i)" 10000
    }
    
    puts "* Doing sanity check on stats"
    dtn::wait_for_bundle_stats 0 "0 pending [expr $N * 2] received $N delivered"
    dtn::wait_for_bundle_stats 1 "0 pending [expr $N * 2] received $N delivered"
    dtn::wait_for_bundle_stats 0 "0 pending $N transmitted"
    dtn::wait_for_bundle_stats 1 "0 pending $N transmitted"

    puts "* Resetting stats"
    dtn::tell_dtnd * bundle reset_stats

    puts "* Clearing the read_delay"
    dtn::tell_dtnd 0 link reconfigure $cl-link:0-1 \
	    test_read_delay=0 block_length=[expr 100 * 1024]
    dtn::tell_dtnd 1 link reconfigure $cl-link:1-0 \
	    test_read_delay=0 block_length=[expr 100 * 1024]

    puts "* Upping the block length"
    dtn::tell_dtnd 0 link reconfigure $cl-link:0-1 block_length=[expr 100 * 1024]
    dtn::tell_dtnd 1 link reconfigure $cl-link:1-0 block_length=[expr 100 * 1024]

    puts "* Growing the send/recv buffers"
    dtn::tell_dtnd 0 link reconfigure $cl-link:0-1 sendbuf_len=32768 recvbuf_len=32768
    dtn::tell_dtnd 1 link reconfigure $cl-link:1-0 sendbuf_len=32768 recvbuf_len=32768

    puts "* Sending a 5MB bundle in each direction"
    set timestamp_0 [dtn::tell_dtnd 0 bundle inject dtn://host-0/test dtn://host-1/test "payload" length=[expr 10 * 1024 * 1024] ]
    set timestamp_1 [dtn::tell_dtnd 1 bundle inject dtn://host-1/test dtn://host-0/test "payload" length=[expr 10 * 1024 * 1024] ]

    puts "* Waiting for delivery of the bundles"
    dtn::wait_for_bundle 0 "dtn://host-1/test,$timestamp_1" 60000
    dtn::wait_for_bundle 1 "dtn://host-0/test,$timestamp_0" 60000
    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
