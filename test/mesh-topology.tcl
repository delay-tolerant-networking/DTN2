#
# set up routing...
#
proc setup_mesh_topology { daemonId port num_nodes } {

    route set type "static"
    set peer [info hostname]
    set myport $port
    
    for {set myid 0} {$myid < $num_nodes} {incr myid} {
	set next_hop $peer:$myport
	# tcp route 
	if { $daemonId != $myid } {	    
	    route add bundles://internet/host://$peer bundles://internet/tcp://$next_hop ONDEMAND
	    route add bundles://internet/host://$peer/* bundles://internet/tcp://$next_hop ONDEMAND
	}
	incr myport	
    }
	
}
