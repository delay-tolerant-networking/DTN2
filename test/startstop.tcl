test::name startstop
net::num_nodes 1

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

test::script {
    puts "***"
    puts "*** test script starting ***"
    puts "***"
    dtn::run_node 0

    dtn::wait_for_dtnd *
    
    after 2000
    dtn::tell 0 exit
    puts "*** test script complete ***"
}
