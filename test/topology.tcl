#
# Set up a linear topology.
#
proc setup_linear_topology {type cl {args ""}} {
    global hosts ports num_nodes id route_to_root

    log /test info "setting up linear topology"

    # tcp route to next_hop in chain. can just use the peer
    if { $id != [expr $num_nodes - 1] } {
	set peerid [expr $id + 1]
	set peeraddr $hosts($peerid)
	set peerport $ports($cl,$peerid)

	eval link add link-$peerid host://$peeraddr:$peerport \
		$type $cl $args 

	route add bundles://internet/host://host-$peerid/* link-$peerid
    }

    # and previous a hop in chain as well
    if { $id != 0 } {
	set peerid [expr $id - 1]
	set peeraddr $hosts($peerid)
	set peerport $ports($cl,$peerid)
	
	eval link add link-$peerid host://$peeraddr:$peerport \
		$type $cl $args

	route add bundles://internet/host://host-$peerid/* link-$peerid

	set route_to_root bundles://internet/host://$peeraddr
    }
}

#
# Set up a tree-based routing topology on nodes 0-254
#
#                                 0
#                                 |
#           /--------------/-----/|\----\--------------\
#          1              2     .....    8              9
#   /---/-----\---\      /-\            /-\      /---/-----\---\
#  10  11 ... 18  19     ...            ...     90  91 ... 98  99
#  |   |  ...  |   |                            |   |       |   |
# 110 111     118 119                          190 191     198 199
#  |   |       |   |  
# 210 211     218 219
#
proc setup_tree_topology {type cl {args ""}} {
    global hosts ports num_nodes id route_to_root

    # the root has routes to all 9 first-hop descendents
    if { $id == 0 } {
	for {set child 1} {$child <= 9} {incr child} {
	    set childaddr $hosts($child)
	    set childport $ports($cl,$child)
	    eval link add link-$child host://$childaddr:$childport \
		    $type $cl $args
	    
	    route add bundles://internet/host://host-$child/* link-$child
	}
    }

    # otherwise, the first hop nodes have routes to the root and their
    # 9 second tier descendents
    if { $id >= 1 && $id <= 9 } {
	set parent 0
	set parentaddr $hosts($parent)
	set parentport $ports($cl,$parent)
	eval link add link-$parent host://$parentaddr:$parentport \
		$type $cl $args
	
	route add bundles://internet/host://host-$parent/* link-$parent
	set route_to_root bundles://internet/host://$parentaddr
	
	for {set child [expr $id * 10]} \
		{$child <= [expr ($id * 10) + 9]} \
		{incr child}
	{
	    set childaddr $hosts($child)
	    set childport $ports($cl,$child)
	    eval link add link-$child host://$childaddr:$childport \
		    $type $cl $args
	    
	    route add bundles://internet/host://host-$child/* link-$child
	}
    }

    # for third-tier nodes 100-199, set their upstream route. for
    # 100-154, can also set a fourth-tier route to 200-254
    if { $id >= 100 && $id <= 199 } {
	set parent [expr $id / 10]
	set parentaddr $hosts($parent)
	set parentport $ports($cl,$parent)
	eval link add link-$parent host://$parentaddr:$parentport \
		$type $cl $args
	
	route add bundles://internet/host://host-$parent/* link-$parent
	set route_to_root bundles://internet/host://$parentaddr

	if {$id <= 154} {
	    set child [expr $id + 100]
	    set childaddr $hosts($child)
	    set childport $ports($cl,$child)
	    eval link add link-$child host://$childaddr:$childport \
		    $type $cl $args
	    
	    route add bundles://internet/host://host-$child/* link-$child
	}
    }

    # finally, the fourth tier nodes 200-254 just set a route to their parent
    if { $id >= 200 && $id <= 255 } {
	set parent [expr $id - 100]
	set parentaddr $hosts($parent)
	set parentport $ports($cl,$parent)
	eval link add link-$parent host://$parentaddr:$parentport \
		$type $cl $args
	
	route add bundles://internet/host://host-$parent/* link-$parent
	set route_to_root bundles://internet/host://$parentaddr
    }
}
