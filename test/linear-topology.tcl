#
# set up routing...
#
proc setup_linear_topology { daemonId port num_nodes } {

    route set type "static"
    set peer [info hostname]
    set port [expr $port + $daemonId]
      
    # tcp route to next_hop in chain
    if { $daemonId != [expr $num_nodes - 1] } {
	# our next peer has port one larger than us
	set next_hop $peer:[expr $port + 1]
	route add bundles://internet/host://$peer bundles://internet/tcp://$next_hop ONDEMAND
	route add bundles://internet/host://$peer/* bundles://internet/tcp://$next_hop ONDEMAND	
    }
    # set previous hops in chain as well
    if { $daemonId != 0 } {
	set prev_hop $peer:[expr $port - 1]
	route add bundles://internet/host://$peer bundles://internet/tcp://$prev_hop ONDEMAND
	route add bundles://internet/host://$peer/* bundles://internet/tcp://$prev_hop ONDEMAND 	
    }
	
}
