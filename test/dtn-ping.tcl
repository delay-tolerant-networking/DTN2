test::name dtn-ping
net::num_nodes 1

manifest::file apps/dtnping/dtnping dtnping

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

test::script {

    # XXX set this to 30 or something once we figure out why the DTND
    # bundle stats are so screwy
    set num_pings 3
    
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set last_node [expr [net::num_nodes] - 1]
    set source    [dtn::tell_dtnd $last_node {route local_eid}]
    set dest      dtn://host-0/test
    
    dtn::tell_dtnd 0 tcl_registration $dest

    puts "* Starting dtnping for $num_pings seconds"
    dtn::run_app 0 dtnping "-c $num_pings $net::host(0)"

    after [expr ($num_pings * 1000) + 1000]

    # XXX/todo check stats here:

    puts "DTN daemon stats: [dtn::tell_dtnd 0 bundle stats]"
    puts "XXX still need to check the stats once "
    
    # XXX shtudown ping here?
    puts "* Test success!"
    
    
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::tell_dtnd * shutdown
}
