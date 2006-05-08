test::name custody-transfer
net::num_nodes 4

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology OPPORTUNISTIC tcp true

test::script {
    puts "* running dtnds"
    dtn::run_dtnd *
    
    puts "* waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    set N [net::num_nodes]
    set src dtn://host-0/test
    set dst dtn://host-[expr $N - 1]/test
    set src_route dtn://host-0/*
    set dst_route dtn://host-[expr $N - 1]/*
    
    for {set i 0} {$i < $N} {incr i} {
	dtn::tell_dtnd $i tcl_registration dtn://host-$i/test
    }

    puts "*"
    puts "* Test 1: basic custody transfer"
    puts "*"
    
    puts "* sending bundle with custody transfer"
    set timestamp [dtn::tell_dtnd 0 sendbundle $src $dst custody]

    puts "* checking that node 0 got the bundle"
    dtn::wait_for_bundle_stats 0 {1 received 0 transmitted}
    dtn::wait_for_bundle_stats 1 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 2 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 received 0 transmitted}

    puts "* checking that node 0 took custody"
    dtn::wait_for_bundle_stats 0 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 1 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    puts "* opening backward links"
    dtn::tell_dtnd 3 link open tcp-link:3-2
    dtn::tell_dtnd 2 link open tcp-link:2-1
    dtn::tell_dtnd 1 link open tcp-link:1-0

    dtn::wait_for_link_state 3 tcp-link:3-2 OPEN
    dtn::wait_for_link_state 2 tcp-link:2-1 OPEN
    dtn::wait_for_link_state 1 tcp-link:1-0 OPEN

    puts "* opening link from 0-1"
    dtn::tell_dtnd 0 link open tcp-link:0-1
    dtn::wait_for_link_state 0 tcp-link:0-1 OPEN

    puts "* checking that custody was transferred"
    dtn::wait_for_bundle_stats 0 {2 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 1 generated 1 transmitted}
    dtn::wait_for_bundle_stats 2 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 received 0 transmitted}
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    puts "* open the rest of the links, check delivery"
    dtn::tell_dtnd 1 link open tcp-link:1-2
    dtn::tell_dtnd 2 link open tcp-link:2-3

    dtn::wait_for_bundle_stats 3 {1 delivered}
    dtn::wait_for_bundle 3 "$src,$timestamp" 30000
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    puts "*"
    puts "* Test 2: custody timer retransmission"
    puts "*"

    dtn::tell_dtnd * bundle reset_stats

    set custody_timer_opts "custody_timer_base=5 custody_timer_lifetime_pct=0"

    puts "* removing route from node 1"
    dtn::tell_dtnd 1 route del $dst_route

    puts "* sending another bundle, checking that node 1 has custody"
    set timestamp [dtn::tell_dtnd 0 sendbundle $src $dst custody]
    
    dtn::wait_for_bundle_stats 0 {2 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 1 generated 1 transmitted}
    dtn::wait_for_bundle_stats 2 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 received 0 transmitted}
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    puts "* adding route to null on node 1, checking transmitted"
    eval dtn::tell_dtnd 1 route add $dst_route null $custody_timer_opts

    dtn::wait_for_bundle_stats 0 {2 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 1 generated 2 transmitted}
    dtn::wait_for_bundle_stats 2 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 received 0 transmitted}
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    puts "* waiting for a couple retransmissions"

    dtn::wait_for_bundle_stats 0 {2 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 4 transmitted}
    dtn::wait_for_bundle_stats 2 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 received 0 transmitted}
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    puts "* switching route back, waiting for retransmission and delivery"
    dtn::tell_dtnd 1 route del $dst_route
    eval dtn::tell_dtnd 1 route add $dst_route tcp-link:1-2 $custody_timer_opts

    dtn::wait_for_bundle_stats 0 {2 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {2 received 5 transmitted}
    dtn::wait_for_bundle_stats 2 {2 received 2 transmitted}
    dtn::wait_for_bundle_stats 3 {1 received 1 transmitted}
    
    dtn::wait_for_bundle_stats 3 {1 delivered}
    dtn::wait_for_bundle 3 "$src,$timestamp" 30000
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    puts "*"
    puts "* Test 3: duplicate detection"
    puts "*"

    dtn::tell_dtnd * bundle reset_stats

    set custody_timer_opts "custody_timer_base=5 custody_timer_lifetime_pct=0"

    puts "* speeding up custody timer for route on node 0"
    dtn::tell_dtnd 0 route del $dst_route
    eval dtn::tell_dtnd 0 route add $dst_route tcp-link:0-1 $custody_timer_opts
    
    puts "* removing destination route for node 1"
    dtn::tell_dtnd 1 route del $dst_route
    
    puts "* switching reverse route for node 1 to null"
    dtn::tell_dtnd 1 route del $src_route
    dtn::tell_dtnd 1 route add $src_route null

    puts "* sending another bundle"
    set timestamp [dtn::tell_dtnd 0 sendbundle $src $dst custody]

    puts "* checking that node 1 and node 0 both have custody"
    dtn::wait_for_bundle_stats 0 {1 received 1 transmitted}
    dtn::wait_for_bundle_stats 1 {1 received 1 generated 1 transmitted}
    dtn::wait_for_bundle_stats 2 {0 received 0 transmitted}
    dtn::wait_for_bundle_stats 3 {0 received 0 transmitted}
    
    dtn::wait_for_bundle_stats 0 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}

    puts "* waiting for a retransmission, making sure duplicate is deleted"
    dtn::wait_for_bundle_stats 0 {1 received 2 transmitted}
    dtn::wait_for_bundle_stats 1 {2 received 2 generated 2 transmitted}
    
    dtn::wait_for_bundle_stats 0 {1 pending 1 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}

    puts "* flipping back the route from 1 to 0"
    dtn::tell_dtnd 1 route del $src_route
    dtn::tell_dtnd 1 route add $src_route tcp-link:1-0

    puts "* waiting for another retransmission, checking custody transferred"
    dtn::wait_for_bundle_stats 0 {2 received 3 transmitted}
    dtn::wait_for_bundle_stats 1 {3 received 3 generated 3 transmitted}
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {1 pending 1 custody}

    puts "* adding route back to 1 and waiting for delivery"
    dtn::tell_dtnd 1 route del $dst_route
    dtn::tell_dtnd 1 route add $dst_route tcp-link:1-2

    dtn::wait_for_bundle_stats 3 {1 delivered}
    dtn::wait_for_bundle 3 "$src,$timestamp" 30000
    
    dtn::wait_for_bundle_stats 0 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 1 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 2 {0 pending 0 custody}
    dtn::wait_for_bundle_stats 3 {0 pending 0 custody}
    
    
    # XXX/TODO: add support / test cases for:
    #
    # delivery before anyone takes custody
    # restart posts a new custody timer
    # noroute timer
    # no contact timer
    # retransmission over an ONDEMAND link that's gone idle
    # multiple routes, ensure retransmission only on one
    # race between bundle delete and custody timer cancelling
    
    puts "* test success!"
}

test::exit_script {
    puts "* stopping all dtnds"
    dtn::stop_dtnd *
}
