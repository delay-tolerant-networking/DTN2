test::name startstop
net::default_num_nodes 1

dtn::config
dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

test::script {
    puts "* Startstop test script executing..."
    dtn::run_dtnd *
    puts "* Waiting for dtnd"
    dtn::wait_for_dtnd *
    puts "* Exiting dtn daemons"
    dtn::tell_dtnd * {puts "startstop test complete"}
    dtn::tell_dtnd * {exit}
    puts "* Startstop test script complete"
}
