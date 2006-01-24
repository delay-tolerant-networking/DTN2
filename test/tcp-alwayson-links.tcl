test::name tcp-alwayson-links
net::num_nodes 3

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ALWAYSON tcp true \
	min_retry_interval=1 max_retry_interval=10 rtt_timeout=1000

test::script {
    puts "* running dtnds"
    dtn::run_dtnd *

    puts "* waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set source    [dtn::tell_dtnd 2 route local_eid]
    set dest      dtn://host-0/test

    dtn::tell_dtnd 0 tcl_registration $dest

    puts "* checking that link is open"
    dtn::wait_for_link_state 2 tcp-link:2-1 OPEN

    puts "* sending bundle"
    set timestamp [dtn::tell_dtnd 2 sendbundle $source $dest]

    puts "* waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 5000
    
    puts "* killing daemon 1"
    dtn::stop_dtnd 1
    
    puts "* checking that link is unavailable"
    dtn::wait_for_link_state 2 tcp-link:2-1 UNAVAILABLE

    puts "* waiting for a few retry timers"
    after 10000
    
    puts "* checking that link is still UNAVAILABLE"
    dtn::wait_for_link_state 2 tcp-link:2-1 UNAVAILABLE
    
    puts "* restarting daemon 1"
    dtn::run_dtnd 1
    dtn::wait_for_dtnd 1
    
    puts "* checking that link is OPEN"
    dtn::wait_for_link_state 2 tcp-link:2-1 OPEN

    puts "* sending bundle"
    set timestamp [dtn::tell_dtnd 2 sendbundle $source $dest]

    puts "* waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 5000
    
    puts "* waiting for the idle timer, making sure the link is open"
    after 10000
    dtn::wait_for_link_state 2 tcp-link:2-1 OPEN

    puts "* forcibly closing the link"
    dtn::tell_dtnd 2 link close tcp-link:2-1
    dtn::wait_for_link_state 2 tcp-link:2-1 UNAVAILABLE

    puts "* re-opening the link"
    dtn::tell_dtnd 2 link open tcp-link:2-1
    dtn::wait_for_link_state 2 tcp-link:2-1 OPEN

    puts "* sending another bundle, checking that the link stays closed"
    set timestamp [dtn::tell_dtnd 2 sendbundle $source $dest]

    puts "* waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 5000

    puts "* test success!"
}

test::exit_script {
    puts "* stopping all dtnds"
    dtn::stop_dtnd *
}
