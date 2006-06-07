test::name inflight-expiration
net::num_nodes 2

dtn::config

puts "* Configuring tcp interfaces / links"
dtn::config_interface tcp
dtn::config_linear_topology ALWAYSON tcp true \
	"test_write_delay=1000 writebuf_len=1024"

test::script {
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    puts "* Setting up flamebox-ignore"
    dtn::tell_dtnd 1 log /test always \
	    "flamebox-ignore ign1 scheduling IMMEDIATE expiration"

    puts "* Waiting for link to open"
    dtn::wait_for_link_state 0 tcp-link:0-1 OPEN

    set source dtn://host-0/test
    set dest   dtn://host-1/test
    
    dtn::tell_dtnd 1 tcl_registration $dest
    
    puts "* Sending bundle"
    set timestamp [dtn::tell_dtnd 0 sendbundle $source $dest \
	    length=5000 expiration=2]
    
    puts "* Waiting for bundle expiration at source"
    dtn::wait_for_bundle_stats 0 {1 expired}
    
    puts "* Checking that bundle is still in flight"
    dtn::check_link_stats 0 tcp-link:0-1 {1 bundles_inflight}
    
    puts "* Checking that its not pending"
    dtn::wait_for_bundle_stats 0 {0 pending}
    dtn::wait_for_bundle_stats 1 {0 pending}
    
    puts "* Checking that it was received but not delivered"
    dtn::wait_for_bundle_stats 1 {1 received 1 expired 0 delivered}
    
    puts "* Test success!"
}

test::exit_script {
    puts "* clearing flamebox ignores"
    tell_dtnd 1 log /test always "flamebox-ignore-cancel ign1"
    
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}