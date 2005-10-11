test::name test-link-updown
net::num_nodes 3

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true \
	connect_retry=2000 connect_timeout=4000 \
	min_retry_interval=5 max_retry_interval=10

test::script {
    puts "* running dtnds"
    dtn::run_dtnd *

    puts "* waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set last_node [expr [net::num_nodes] - 1]
    set source    [dtn::tell_dtnd $last_node route local_eid]
    set dest      dtn://host-0/test

    dtn::tell_dtnd 0 tcl_registration $dest

    puts "* sending bundle"
    set timestamp [dtn::tell_dtnd $last_node sendbundle $source $dest]

    puts "* waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 5000
    
    puts "* sanity checking stats"
    dtn::wait_for_stat 0 1 received 5000

    puts "* checking that link is open"
    dtn::check_link_state $last_node link-1 OPEN

    puts "* killing daemon 1"
    dtn::tell_dtnd 1 {exit}
    
    puts "* checking that link is unavailable"
    dtn::wait_for_link_state $last_node link-1 UNAVAILABLE

    puts "* waiting for link to become available"
    dtn::wait_for_link_state $last_node link-1 AVAILABLE

    puts "* sending another bundle"
    set timestamp [dtn::tell_dtnd $last_node sendbundle $source $dest]

    puts "* checking that link is now UNAVAILABLE"
    dtn::wait_for_link_state $last_node link-1 UNAVAILABLE

    puts "* waiting for a few retry timers"
    after 30000
    
    puts "* checking that link is still UNAVAILABLE"
    dtn::wait_for_link_state $last_node link-1 UNAVAILABLE
    
    puts "* restarting daemon 1"
    dtn::run_dtnd 1
    dtn::wait_for_dtnd 1
    
    puts "* checking that link is AVAILABLE"
    dtn::wait_for_link_state $last_node link-1 UNAVAILABLE

    puts "* waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 5000
    
    puts "* sanity checking stats"
    dtn::wait_for_stat 0 2 received 5000

    puts "* test success!"
}

test::exit_script {
    puts "* stopping all dtnds"
    dtn::tell_dtnd * shutdown
}
