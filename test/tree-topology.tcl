#
# set up routing...
#
proc setup_tree_topology { daemonId port num_nodes } {

    route set type "static"
    set peer [info hostname]
    
    #create parent
    set parent 0

    # Daemon Id of parent nodes
    if { $daemonId != 0 } {
	if { [expr $daemonId%2] == 0 } {
	    set parent [expr $port + $daemonId/2 - 1]
	} else {
	    set parent [expr $port + (($daemonId + 1)/2) - 1]
	}
	route add bundles://internet/host://$peer/* bundles://internet/tcp://$peer:$parent ONDEMAND	
	route add bundles://internet/host://$peer bundles://internet/tcp://$peer:$parent ONDEMAND
    }

    # Respective ports of the children nodes
    set child1 [expr $port + $daemonId * 2 + 1]
    set child2 [expr $port + $daemonId * 2 + 2]

    # Set the route table routes for the nodes
    if { $child1 <= [expr $port + $num_nodes - 1] } {
	route add bundles://internet/host://$peer bundles://internet/tcp://$peer:$child1 ONDEMAND
	route add bundles://internet/host://$peer/* bundles://internet/tcp://$peer:$child1 ONDEMAND
    }
    if { $child2 <= [expr $port + $num_nodes - 1] } {
	route add bundles://internet/host://$peer bundles://internet/tcp://$peer:$child2 ONDEMAND	
	route add bundles://internet/host://$peer/* bundles://internet/tcp://$peer:$child2 ONDEMAND
    }

}
