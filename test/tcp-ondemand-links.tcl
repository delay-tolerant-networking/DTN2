test::name tcp-ondemand-links
net::num_nodes 3

dtn::config

dtn::config_interface tcp

set retry_opts ""
for {set i 0} {$i < [llength $opt(opts)]} {incr i} {
    set var [lindex $opt(opts) $i]

    if {$var == "-fast_retries"} {
	set retry_opts "min_retry_interval=1 max_retry_interval=10 rtt_timeout=1000"
    } else {
	error "unknown test option $var"
    }
}

eval dtn::config_linear_topology ONDEMAND tcp true \
	$retry_opts idle_close_time=5

test::script {
    puts "* running dtnds"
    dtn::run_dtnd *

    puts "* waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set source    [dtn::tell_dtnd 2 route local_eid]
    set dest      dtn://host-0/test

    dtn::tell_dtnd 0 tcl_registration $dest

    puts "* checking that link is available "
    dtn::wait_for_link_state 2 tcp-link:2-1 AVAILABLE
    
    puts "* sending bundle"
    set timestamp [dtn::tell_dtnd 2 sendbundle $source $dest]

    puts "* waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 5000
    
    puts "* sanity checking stats"
    dtn::wait_for_bundle_stat 0 1 received 5000

    puts "* checking that link is open"
    dtn::wait_for_link_state 2 tcp-link:2-1 OPEN

    puts "* killing daemon 1"
    dtn::stop_dtnd 1
    
    puts "* checking that link is unavailable"
    dtn::wait_for_link_state 2 tcp-link:2-1 UNAVAILABLE

    puts "* sending another bundle"
    set timestamp [dtn::tell_dtnd 2 sendbundle $source $dest]

    puts "* waiting for a few retry timers"
    after 10000
    
    puts "* checking that link is still UNAVAILABLE"
    dtn::wait_for_link_state 2 tcp-link:2-1 UNAVAILABLE
    
    puts "* restarting daemon 1"
    dtn::run_dtnd 1
    dtn::wait_for_dtnd 1
    
    puts "* checking that link is AVAILABLE or OPEN"
    dtn::wait_for_link_state 2 tcp-link:2-1 {AVAILABLE OPEN}

    puts "* waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 5000
    
    puts "* sanity checking stats"
    dtn::wait_for_bundle_stat 0 2 received 5000

    puts "* waiting for the idle timer to close the link"
    dtn::wait_for_link_state 2 tcp-link:2-1 AVAILABLE

    puts "* forcibly setting the link to unavailable"
    dtn::tell_dtnd 2 link set_available tcp-link:2-1 false
    dtn::wait_for_link_state 2 tcp-link:2-1 UNAVAILABLE

    puts "* sending another bundle, checking that the link stays closed"
    set timestamp [dtn::tell_dtnd 2 sendbundle $source $dest]
    dtn::wait_for_link_state 2 tcp-link:2-1 UNAVAILABLE

    puts "* resetting the link to available, checking that it opens"
    dtn::tell_dtnd 2 link set_available tcp-link:2-1 true
    dtn::wait_for_link_state 2 tcp-link:2-1 OPEN
    
    puts "* waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 5000

    puts "* forcibly closing the link"
    dtn::tell_dtnd 2 link close tcp-link:2-1
    dtn::wait_for_link_state 2 tcp-link:2-1 UNAVAILABLE
    
    puts "* resetting the link to available, checking that it's not yet open"
    dtn::tell_dtnd 2 link set_available tcp-link:2-1 true
    dtn::wait_for_link_state 2 tcp-link:2-1 AVAILABLE
    
    puts "* forcibly opening the link"
    dtn::tell_dtnd 2 link open tcp-link:2-1
    dtn::wait_for_link_state 2 tcp-link:2-1 OPEN
    
    puts "* test success!"
}

test::exit_script {
    puts "* stopping all dtnds"
    dtn::stop_dtnd *
}
