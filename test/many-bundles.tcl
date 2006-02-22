test::name many-bundles
net::num_nodes 4
manifest::file apps/dtnrecv/dtnrecv dtnrecv

dtn::config

set clayer tcp
set count  5000
set length 10

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set clayer $val
    } elseif {$var == "-count" || $var == "count"} {
        set count $val	
    } elseif {$var == "-length" || $var == "length"} {
        set length $val	
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

puts "* Configuring $clayer interfaces / links"
dtn::config_interface $clayer
dtn::config_linear_topology OPPORTUNISTIC $clayer true

test::script {
    puts "* Running dtnd 0"
    dtn::run_dtnd 0
    dtn::wait_for_dtnd 0

    set source    dtn://host-0/test
    set dest      dtn://host-3/test
    
    puts "* Sending $count bundles of length $length"
    for {set i 0} {$i < $count} {incr i} {
	set timestamp($i) [dtn::tell_dtnd 0 sendbundle $source $dest\
		length=$length expiration=300]
    }

    puts "* Checking that all the bundles are queued"
    dtn::wait_for_stats 0 "$count pending"

    puts "* Restarting dtnd 0"
    dtn::stop_dtnd 0
    dtn::run_dtnd 0 ""

    puts "* Checking that all bundles were re-read from the database"
    dtn::wait_for_dtnd 0
    dtn::wait_for_stats 0 "$count pending"

    puts "* Starting dtnd 1"
    dtn::run_dtnd 1
    dtn::wait_for_dtnd 1

    puts "* Opening link to dtnd 1"
    dtn::tell_dtnd 0 link open $clayer-link:0-1

    puts "* Waiting for all bundles to flow"
    dtn::wait_for_stats 0 "0 pending" [expr 10000 + ($count * 100)]
    dtn::wait_for_stats 1 "$count pending"  [expr 10000 + ($count * 100)]

    puts "* Starting dtnd 2 and 3"
    dtn::run_dtnd 2
    dtn::run_dtnd 3
    dtn::wait_for_dtnd 2
    dtn::wait_for_dtnd 3

    puts "* Starting dtnrecv on node 3"
    set rcvpid [dtn::run_app 3 dtnrecv "$dest -q -n $count"]

    puts "* Opening links"
    dtn::tell_dtnd 2 link open $clayer-link:2-3
    dtn::tell_dtnd 1 link open $clayer-link:1-2

    puts "* Waiting for all bundles to be delivered"
    dtn::wait_for_stats 1 "0 pending"  [expr 10000 + ($count * 100)]
    dtn::wait_for_stats 2 "0 pending"  [expr 10000 + ($count * 100)]
    dtn::wait_for_stats 3 "$count delivered"  [expr 10000 + ($count * 100)]

    puts "* Waiting for receiver to complete"
    run::wait_for_pid_exit 3 $rcvpid

    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
