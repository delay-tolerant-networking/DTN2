#
# Set up a linear topology.
#
proc setup_linear_topology {type cl {args ""}} {
    global hosts num_nodes id route_to_root

    # tcp route to next_hop in chain. can just use the peer
    if { $id != [expr $num_nodes - 1] } {
	set peerid [expr $id + 1]
	set peeraddr $hosts($peerid)
	eval link add link-$peerid host://$peeraddr:5000 \
		$type $cl $args

	route add bundles://internet/host://$peeraddr/* link-$peerid
    }

    # and previous a hop in chain as well
    if { $id != 0 } {
	set peerid [expr $id - 1]
	set peeraddr $hosts($peerid)
	eval link add link-$peerid host://$peeraddr:5000 \
		$type $cl $args

	route add bundles://internet/host://$peeraddr/* link-$peerid

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
    global hosts num_nodes id route_to_root

    # the root has routes to all 9 first-hop descendents
    if { $id == 0 } {
	for {set child 1} {$child <= 9} {incr child} {
	    set childaddr $hosts($child)
	    eval link add link-$child host://$childaddr:5000 \
		    $type $cl $args
	    
	    route add bundles://internet/host://$childaddr/* link-$child
	}
    }

    # otherwise, the first hop nodes have routes to the root and their
    # 9 second tier descendents
    if { $id >= 1 && $id <= 9 } {
	set parent 0
	eval link add link-$parent host://$hosts($parent):5000 \
		$type $cl $args
	
	route add bundles://internet/host://$hosts($parent)/* link-$parent
	set route_to_root bundles://internet/host://$hosts($parent)
	
	for {set child [expr $id * 10]} {$child <= [expr ($id * 10) + 9]} {incr child} {
	    set childaddr $hosts($child)
	    eval link add link-$child host://$childaddr:5000 \
		    $type $cl $args
	    
	    route add bundles://internet/host://$childaddr/* link-$child
	}
    }

    # for third-tier nodes 100-199, set their upstream route. for
    # 100-154, can also set a fourth-tier route to 200-254
    if { $id >= 100 && $id <= 199 } {
	set parent [expr $id / 10]
	eval link add link-$parent host://$hosts($parent):5000 \
		$type $cl $args
	
	route add bundles://internet/host://$hosts($parent)/* link-$parent
	set route_to_root bundles://internet/host://$hosts($parent)

	if {$id <= 154} {
	    set child [expr $id + 100]
	    set childaddr $hosts($child)
	    eval link add link-$child host://$childaddr:5000 \
		    $type $cl $args
	    
	    route add bundles://internet/host://$childaddr/* link-$child
	}
    }

    # finally, the fourth tier nodes 200-254 just set a route to their parent
    if { $id >= 200 && $id <= 255 } {
	set parent [expr $id - 100]
	eval link add link-$parent host://$hosts($parent):5000 \
		$type $cl $args
	
	route add bundles://internet/host://$hosts($parent)/* link-$parent
	set route_to_root bundles://internet/host://$hosts($parent)
    }
}
