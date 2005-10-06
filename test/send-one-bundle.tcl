test::name send-one-bundle
net::num_nodes 3

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

test::script {
    puts "* running dtnds"
    dtn::run_dtnd *

    puts "* waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    set last_node [expr [net::num_nodes] - 1]
    set source    [dtn::tell_dtnd $last_node {route local_eid}]
    set dest      dtn://host-0/test
    
    dtn::tell_dtnd 0 tcl_registration $dest
    
    puts "* sending bundle"
    set timestamp [dtn::tell_dtnd $last_node sendbundle $source $dest]
    
    puts "* waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 5000

    puts "* checking bundle data"
    dtn::check_bundle_data 0 "$source,$timestamp" \
	    isadmin 0 source $source dest $dest
    
    puts "* doing sanity check on stats"
    dtn::wait_for_stat 0 1 received 5000
    
    puts "* test success!"
}

test::exit_script {
    puts "* stopping all dtnds"
    dtn::tell_dtnd * exit
}
