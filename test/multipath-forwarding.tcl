#
#    Copyright 2006 Intel Corporation
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

test::name multipath-forwarding
net::num_nodes 4

dtn::config

set clayer   tcp

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set clayer $val
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

puts "* Configuring $clayer interfaces / links"
dtn::config_interface $clayer
dtn::config_diamond_topology ALWAYSON $clayer true

test::script {
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    dtn::tell_dtnd 3 tcl_registration dtn://host-3/test
    
    puts "* Waiting for links to open"
    dtn::wait_for_link_state 0 tcp-link:0-1 OPEN
    dtn::wait_for_link_state 0 tcp-link:0-2 OPEN

    puts "* adding write delay to node 0 links"
    tell_dtnd 0 link reconfigure tcp-link:0-1 \
	    test_write_delay=500 sendbuf_len=1024
    tell_dtnd 0 link reconfigure tcp-link:0-2 \
	    test_write_delay=500 sendbuf_len=1024

    puts "*"
    puts "* Test 1: equal size bundles, equal priority routes"
    puts "*"

    puts "* sending 10 bundles"
    for {set i 0} {$i < 10} {incr i} {
	tell_dtnd 0 sendbundle dtn://host-0/test dtn://host-3/test length=4096
    }

    puts "* waiting for all to be transmitted"
    dtn::wait_for_bundle_stats 0 {0 pending 10 transmitted}
    
    puts "* checking they were load balanced"
    dtn::check_link_stats 0 tcp-link:0-1 5 bundles_transmitted
    dtn::check_link_stats 0 tcp-link:0-2 5 bundles_transmitted

    puts "* waiting for delivery"
    dtn::wait_for_bundle_stats 3 {0 pending 10 delivered 0 duplicate}
    
    dtn::tell_dtnd * bundle reset_stats
    
    puts "*"
    puts "* Test 2: equal size bundles, different priority routes"
    puts "*"

    puts "* swapping routes on node 0"
    tell_dtnd 0 route del dtn://host-3/*
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-1 route_priority=100
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-2 route_priority=0
    
    puts "* sending 10 bundles"
    for {set i 0} {$i < 10} {incr i} {
	tell_dtnd 0 sendbundle dtn://host-0/test dtn://host-3/test length=4096
    }

    puts "* waiting for all to be transmitted"
    dtn::wait_for_bundle_stats 0 {0 pending 10 transmitted}
    
    puts "* checking they all went on one link"
    dtn::check_link_stats 0 tcp-link:0-1 10 bundles_transmitted
    dtn::check_link_stats 0 tcp-link:0-2 0  bundles_transmitted

    puts "* waiting for delivery"
    dtn::wait_for_bundle_stats 3 {0 pending 10 delivered 0 duplicate}

    puts "* swapping back routes on node 0"
    tell_dtnd 0 route del dtn://host-3/*
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-1
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-2
    
    dtn::tell_dtnd * bundle reset_stats
    
    puts "*"
    puts "* Test 3: copy routes"
    puts "*"

    puts "* swapping routes on node 0"
    tell_dtnd 0 route del dtn://host-3/*
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-1 route_priority=100 
    tell_dtnd 0 route add dtn://host-3/* tcp-link:0-2 \
	    route_priority=0 action=copy

    puts "* sending 10 bundles (to different destination)"
    for {set i 0} {$i < 10} {incr i} {
	tell_dtnd 0 sendbundle dtn://host-0/test dtn://host-3/test2 length=4096
    }

    puts "* waiting for all to be transmitted"
    dtn::wait_for_bundle_stats 0 {0 pending 20 transmitted}
    
    puts "* checking they all went on both links"
    dtn::check_link_stats 0 tcp-link:0-1 10 bundles_transmitted
    dtn::check_link_stats 0 tcp-link:0-2 10 bundles_transmitted

    puts "* waiting for arrival"
    dtn::wait_for_bundle_stats 3 {10 pending 0 delivered 10 duplicate}

    puts "* adding registration, waiting for delivery"
    dtn::tell_dtnd 3 tcl_registration dtn://host-3/test2
    dtn::wait_for_bundle_stats 3 {0 pending 10 delivered 10 duplicate}
    
    puts "* swapping back routes on node 0"
    tell_dtnd 0 route del dtn://host-3
    tell_dtnd 0 route add dtn://host-3 tcp-link:0-1
    tell_dtnd 0 route add dtn://host-3 tcp-link:0-2
    
    dtn::tell_dtnd * bundle reset_stats
    
    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
