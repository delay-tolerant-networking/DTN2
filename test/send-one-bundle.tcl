test::name send-one-bundle
net::num_nodes 3

dtn::config

set clayer tcp
set length 5000

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set clayer $val
    } elseif {$var == "-length" || $var == "length"} {
        set length $val	
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

puts "* Configuring $clayer interfaces / links"
dtn::config_interface $clayer
dtn::config_linear_topology ONDEMAND $clayer true

test::script {
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    set last_node [expr [net::num_nodes] - 1]
    set source    [dtn::tell_dtnd $last_node {route local_eid}]
    set dest      dtn://host-0/test
    
    dtn::tell_dtnd 0 tcl_registration $dest
    
    puts "* Sending bundle"
    set timestamp [dtn::tell_dtnd $last_node sendbundle $source $dest length=$length]
    
    puts "* Waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 30000

    puts "* Checking bundle data"
    dtn::check_bundle_data 0 "$source,$timestamp" \
	    is_admin 0 source $source dest $dest
    
    puts "* Doing sanity check on stats"
    dtn::wait_for_bundle_stats 0 {0 pending 0 expired}
    dtn::wait_for_bundle_stats 1 {0 pending 0 expired}
    dtn::wait_for_bundle_stats 2 {0 pending 0 expired}
    dtn::wait_for_bundle_stats 0 {1 received}
    dtn::wait_for_bundle_stats 0 {1 received}
    dtn::wait_for_bundle_stats 1 {1 transmitted}
    dtn::wait_for_link_stat 1 $clayer-link:1-0 1 bundles_transmitted
    dtn::wait_for_link_stat 1 $clayer-link:1-0 $length bytes_transmitted
    dtn::wait_for_link_stat 1 $clayer-link:1-0 0 bundles_inflight
    dtn::wait_for_link_stat 1 $clayer-link:1-0 0 bytes_inflight
    
    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
