test::name startstop
net::default_num_nodes 1

dtn::config

test::script {
    puts "* Startstop test script executing..."
    dtn::run_dtnd 0
    
    puts "* Waiting for dtnd"
    dtn::wait_for_dtnd 0
    set dtnpid [dtn::tell_dtnd 0 pid]

    puts "* Sending dtnd pid $dtnpid an abort signal"
    run::kill_pid $net::host(0) $dtnpid ABRT

    puts "* Test complete"
}
