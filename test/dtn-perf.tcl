test::name dtn-perf
net::num_nodes 3

manifest::file apps/dtnperf/dtnperf-server dtnperf-server
manifest::file apps/dtnperf/dtnperf-client dtnperf-client

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

set perftime 60
set delivery_opts ""

set mode "memory"

for {set i 0} {$i < [llength $opt(opts)]} {incr i} {
    set var [lindex $opt(opts) $i]

    if {$var == "-perftime" } {
	set perftime [lindex $opt(opts) [incr i]]

    } elseif {$var == "-forwarding_rcpts" } {
	append delivery_opts "-F "

    } elseif {$var == "-receive_rcpts" } {
	append delivery_opts "-R "

    } elseif {$var == "-payload_len"} {
	set len [lindex $opt(opts) [incr i]]
	append delivery_opts "-p $len "

    } elseif {$var == "-file_payload"} {
	set mode "file"
	
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

if {$mode == "memory"} {
    append delivery_opts "-m "
} else {
    append delivery_opts "-f dtnperf.snd "
}

test::script {
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set N [net::num_nodes]
    set last_node [expr $N - 1]
    set dest      dtn://host-0

    # make sure we properly tell the client and server where to put things
    set rundir [dist::get_rundir $net::host($last_node) $last_node]
    regsub {dtnperf.snd} $delivery_opts "$rundir/dtnperf.snd" delivery_opts
    
    set server_opts "-v -a 100 -d $rundir "
    if {$mode == "memory"} {
	append server_opts "-m"
    }
    set server_pid [dtn::run_app 0 dtnperf-server $server_opts]
    after 1000
    
    puts "* Running dtnperf-client for $perftime seconds"
    set client_pid [dtn::run_app $last_node dtnperf-client \
			"-t $perftime $delivery_opts -d $dest" ]

    # XXX might want to try running dtnperf-client when sending to a
    # non-existent endpoint too, such as:
    # "-t $perftime -m -d # $dest/foo" ]

    for {set i 0} {$i < $perftime} {incr i} {
	for {set id 0} {$id <= $last_node} {incr id} {
	    puts "* Node $id: [dtn::tell_dtnd $id bundle stats]"
	}
	puts ""
	after 1000
    }

    puts "* waiting for dtnperf-client to exit"
    run::wait_for_pid_exit $last_node $client_pid

    puts "* Final stats:"
    for {set id 0} {$id <= $last_node} {incr id} {
	puts "* $id: [dtn::tell_dtnd $id bundle stats]"
    }
    puts ""

    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping dtnperf-server"
    run::kill_pid 0 $server_pid 1
    run::wait_for_pid_exit 0 $server_pid
    
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
