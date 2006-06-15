test::name loopback-link
net::num_nodes 1

dtn::config

set clayer tcp

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set clayer $val
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

dtn::config_topology_common false

puts "* Configuring $clayer interface"
dtn::config_interface $clayer

test::script {
    puts "* Running dtnd"
    dtn::run_dtnd 0

    puts "* Waiting for dtnd to start up"
    dtn::wait_for_dtnd *
    
    puts "* Setting up flamebox-ignore"
    dtn::tell_dtnd 0 log /test always \
	    "flamebox-ignore ign1 scheduling IMMEDIATE expiration"
    
    puts "* Adding a link that points back to itself and a default route"
    eval dtn::tell_dtnd 0 link add loopback-link \
	    "$net::host(0):[dtn::get_port $clayer 0]" ALWAYSON $clayer

    dtn::tell_dtnd 0 route add *:* loopback-link action=copy
    
    puts "* Adding a registration"
    dtn::tell_dtnd 0 tcl_registration dtn://host-0/test
    
    puts "* Sending a bundle"
    dtn::tell_dtnd 0 sendbundle dtn://host-0/source dtn://host-0/test \
	    expiration=1

    puts "* Waiting for it to loop"
    after 5000
    
    puts "* Checking stats"
    dtn::check_bundle_stats 0 0 pending 1 transmitted 2 received 1 delivered
    dtn::check_link_stats 0 loopback-link \
	    0 bundles_inflight 1 bundles_transmitted
    
    puts "* Test success!"
}

test::exit_script {
    puts "* clearing flamebox ignores"
    tell_dtnd 0 log /test always "flamebox-ignore-cancel ign1"
    
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
