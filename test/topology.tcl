#
# set up routing...
#

proc setup_interface {cl} {
    global hosts id
    
    route set local_tuple bundles://internet/://$hosts($id)
    
    interface add $cl bundles://internet/host://$hosts($id):5000
}

proc setup_linear_topology {type cl {args ""}} {
    global hosts num_nodes id

    # tcp route to next_hop in chain. can just use the peer
    if { $id != [expr $num_nodes - 1] } {
	set peerid [expr $id + 1]
	set peeraddr $hosts($peerid)
	eval link add link-$peerid bundles://internet/host://$peeraddr \
		$type $cl $args
	route add bundles://internet/host://$peeraddr/* link-$peerid
    }

    # and previous hop in chain as well, along with a default route
    if { $id != 0 } {
	set peerid [expr $id - 1]
	set peeraddr $hosts($peerid)
	eval link add link-$peerid bundles://internet/host://$peeraddr \
		$type $cl $args

	route add bundles://internet/host://$peeraddr/* link-$peerid

	route add bundles://*/* link-$peerid
    }
}

