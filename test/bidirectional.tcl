test::name bidirectional
net::num_nodes 3

manifest::file apps/dtnsend/dtnsend dtnsend
manifest::file apps/dtnrecv/dtnrecv dtnrecv

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
dtn::config_linear_topology ALWAYSON $cl true

conf::add dtnd * "param set early_deletion false"

# XXX/demmer do this test with bigger bundles...

test::script {
    set N [net::num_nodes]
    set last [expr $N - 1]

    set eid1 dtn://host-0/test
    set eid2 dtn://host-$last/test

    set count 200
    set sleep 100

    # ----------------------------------------------------------------------
    puts "* "
    puts "* Test phase 1: continuous connectivity"
    puts "* "

    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    puts "* Running senders / receivers for $count bundles, sleep $sleep"
    set rcvpid1 [dtn::run_app 0     dtnrecv "$eid1 -q -n $count"]
    set rcvpid2 [dtn::run_app $last dtnrecv "$eid2 -q -n $count"]
    set sndpid1 [dtn::run_app 0     dtnsend "-s $eid1 -d $eid2 -t d -z $sleep -n $count"]
    set sndpid2 [dtn::run_app $last dtnsend "-s $eid2 -d $eid1 -t d -z $sleep -n $count"]

    puts "* Waiting for senders / receivers to complete"
    run::wait_for_pid_exit 0     $sndpid1
    run::wait_for_pid_exit $last $sndpid2
    run::wait_for_pid_exit 0     $rcvpid1
    run::wait_for_pid_exit $last $rcvpid2
    
    foreach node [list 0 $last] {
	puts "* Checking bundle stats on node $node"
	dtn::check_bundle_stats $node \
		$count "delivered" \
		[expr $count * 2] "received"
    }

    for {set i 0} {$i < $N} {incr i} {
	puts "Node $i: [dtn::tell_dtnd $i bundle stats]"
    }

    puts "* Checking link stats"
    dtn::check_link_stats 0 $cl-link:0-1 1 contacts 200 bundles_transmitted
    dtn::check_link_stats 1 $cl-link:1-0 1 contacts

    puts "* Stopping dtnds"
    dtn::stop_dtnd *

    # ----------------------------------------------------------------------
    puts "* "
    puts "* Test phase 2: interrupted links"
    puts "* "

    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    puts "* Running senders / receivers for $count bundles, sleep $sleep"
    set rcvpid1 [dtn::run_app 0     dtnrecv "$eid1 -q -n $count"]
    set rcvpid2 [dtn::run_app $last dtnrecv "$eid2 -q -n $count"]
    set sndpid1 [dtn::run_app 0     dtnsend "-s $eid1 -d $eid2 -t d -z $sleep -n $count"]
    set sndpid2 [dtn::run_app $last dtnsend "-s $eid2 -d $eid1 -t d -z $sleep -n $count"]

    for {set i 0} {$i < 20} {incr i} {
	after [expr int(2000 * rand())]
	puts "* Closing links to/from node 1"
	tell_dtnd 0 link close $cl-link:0-1
	tell_dtnd 1 link close $cl-link:1-2

	after [expr int(2000 * rand())]
	puts "* Opening links to/from node 1"
	tell_dtnd 0 link open $cl-link:0-1
	tell_dtnd 1 link open $cl-link:1-2
    }

    puts "* Waiting for senders / receivers to complete"
    run::wait_for_pid_exit 0     $sndpid1
    run::wait_for_pid_exit $last $sndpid2
    run::wait_for_pid_exit 0     $rcvpid1
    run::wait_for_pid_exit $last $rcvpid2
     
    foreach node [list 0 $last] {
	puts "* Checking bundle stats on node $node"
	dtn::check_bundle_stats $node \
		[expr $count * 2] "pending" \
		$count "transmitted" \
		$count "delivered"
    }

    for {set i 0} {$i < $N} {incr i} {
	puts "Node $i: [dtn::tell_dtnd $i bundle stats]"
    }

    puts "* Stopping dtnds"
    dtn::stop_dtnd *

    # ----------------------------------------------------------------------

    puts "* "
    puts "* Test phase 3: killing node"
    puts "* "

    puts "* Running dtnds"
    for {set i 0} {$i < [net::num_nodes]} {incr i} {
	set pids($i) [dtn::run_dtnd $i]
    }

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    puts "* Running senders / receivers for $count bundles, sleep $sleep"
    set rcvpid1 [dtn::run_app 0     dtnrecv "$eid1 -q"]
    set rcvpid2 [dtn::run_app $last dtnrecv "$eid2 -q"]
    set sndpid1 [dtn::run_app 0     dtnsend "-s $eid1 -d $eid2 -t d -z $sleep -n $count"]
    set sndpid2 [dtn::run_app $last dtnsend "-s $eid2 -d $eid1 -t d -z $sleep -n $count"]

    for {set i 0} {$i < 20} {incr i} {

	if {[dtn::test_bundle_stats 0 [list $count "delivered"]] && \
		[dtn::test_bundle_stats 1 [list $count "delivered"]]} {
	    break; # all done
	}

	set sleep [expr int(5000 * rand())]
	puts "* Sleeping for $sleep msecs"
	after $sleep
	
	puts "* Killing node 1"
	tell::close_socket $net::host(1) [dtn::get_port console 1]
	run::kill_pid 1 $pids(1) KILL	

	set sleep [expr int(5000 * rand())]
	puts "* Sleeping for $sleep msecs"
	after $sleep

	puts "* Starting node 1"
	set pids(1) [dtn::run_dtnd 1 ""]
	dtn::wait_for_dtnd 1
    }

    puts "* Done with killing loop.. stats:"
    for {set i 0} {$i < $N} {incr i} {
	puts "Node $i: [tell_dtnd $i bundle stats]"
    }

    puts "* Waiting for senders to complete"
    run::wait_for_pid_exit 0     $sndpid1
    run::wait_for_pid_exit $last $sndpid2

    puts "* Killing receivers"
    run::kill_pid 0     $rcvpid1 TERM
    run::kill_pid $last $rcvpid1 TERM

    # all we can really check for is that they all got transmitted,
    # since some might end up getting lost when the node gets killed,
    # but after the ack was sent
    foreach node [list 0 $last] {
	puts "* Checking bundle stats on node $node"
	dtn::check_bundle_stats $node $count "transmitted"
    }

    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *

}
