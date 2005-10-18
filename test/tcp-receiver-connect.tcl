#
# Test script for the TCP convergence layer's receiver_connect option.
# Used to enable reception of bundles across a firewall / NAT router.
#

test::name tcp-receiver-connect
net::num_nodes 2

dtn::config
dtn::config_topology_common

set host0 $net::host(0)
set host1 $net::host(1)

set port0 [dtn::get_port tcp 0]
set port1 [dtn::get_port tcp 1]

conf::add dtnd 0 "interface add tcp0 tcp \
	receiver_connect \
	local_addr=$host0 local_port=$port0 \
	remote_addr=$host1 remote_port=$port1"

conf::add dtnd 1 "interface add tcp0 tcp \
	local_addr=$host1 local_port=$port1"

conf::add dtnd 1 "link add link-0 $host0:$port0 OPPORTUNISTIC tcp \
	remote_addr=$host0 remote_port=$port0 \
	receiver_connect"

conf::add dtnd 1 "route add dtn://host-0/* link-0"

test::script {
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set source    dtn://host-1/test
    set dest      dtn://host-0/test

    puts "* Setting up registration"
    dtn::tell_dtnd 0 tcl_registration $dest
    
    puts "* Waiting for link to open"
    dtn::wait_for_link_state 1 link-0 OPEN 5000

    puts "* Sending bundle"
    set timestamp [dtn::tell_dtnd 1 sendbundle $source $dest]
    
    puts "* Waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 5000

    puts "* Checking bundle data"
    dtn::check_bundle_data 0 "$source,$timestamp" \
	    isadmin 0 source $source dest $dest
    
    puts "* Doing sanity check on stats"
    dtn::wait_for_stat 0 1 received 5000
    
    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::tell_dtnd * shutdown
}
