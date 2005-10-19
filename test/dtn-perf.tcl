test::name dtn-perf
net::num_nodes 3

manifest::file apps/dtnperf/dtnperf-server dtnperf-server
manifest::file apps/dtnperf/dtnperf-client dtnperf-client

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

global perftime
set perftime 60
foreach {var val} $opt(opts) {
    if {$var == "-perftime" } {
	set perftime $val
	
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

test::script {
    global perftime

    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set N [net::num_nodes]
    set last_node [expr $N - 1]
    set dest      dtn://host-0

    # XXX hacky -d /tmp specification because dtnperf-server is too
    # braindead to not care about creating the directory even when
    # bundles are saved into memory (-m), and I don't feel like
    # touching that code
    set server_pid [dtn::run_app 0 dtnperf-server "-m -v -d /tmp"]
    after 1000

    puts "* Running dtnperf-client for $perftime seconds"
    set client_pid [dtn::run_app $last_node dtnperf-client \
			"-t $perftime -m -d $dest" ]
# XXX might want to try running dtnperf-client when sending to a non-existent endpoint too, such as:    
# 			"-t $perftime -m -d $dest/foo" ]
    after [expr $perftime * 1000]

    run::wait_for_pid_exit $last_node $client_pid

    puts "* Node 0 stats: [dtn::tell_dtnd 0 bundle stats]"
    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping dtnperf-server"
    
    # XXX wrap flamebox ignores around here if necessary:
    run::kill_pid 0 $server_pid 1
    
    puts "* Stopping all dtnds"
    dtn::tell_dtnd * shutdown
}
