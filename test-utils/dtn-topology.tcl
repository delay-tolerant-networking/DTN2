#
# Default config shared by all these topologies
#
proc config_topology_common {} {
    foreach id [net::nodelist] {
	conf::add dtnd $id "route set type static"
	conf::add dtnd $id "route local_eid dtn://host-$id"
    }
}

#
# Set up a linear topology using TCP or UDP
#
proc config_linear_topology {type cl with_routes {args ""}} {
    config_topology_common
    
    set last [expr [net::num_nodes] - 1]
    foreach id [net::nodelist] {
	# link to next hop in chain (except for last one)
	if { $id != $last } {
	    set peerid   [expr $id + 1]
	    set peeraddr $net::host($peerid)
	    set peerport [dtn_port $cl $peerid]

	    conf::add dtnd $id [eval list link add link-$peerid  \
		    $peeraddr:$peerport $type $cl $args]

	    if {$with_routes} {
		for {set dest $peerid} {$dest <= $last} {incr dest} {
		    conf::add dtnd $id \
			    "route add dtn://host-$dest/* link-$peerid"
		}
	    }
	}
	
	# and previous a hop in chain as well
	if { $id != 0 } {
	    set peerid   [expr $id - 1]
	    set peeraddr $net::host($peerid)
	    set peerport [dtn_port $cl $peerid]
	    
	    conf::add dtnd $id [eval list link add link-$peerid \
		    $peeraddr:$peerport  $type $cl $args]
	    
	    if {$with_routes} {
		for {set dest $peerid} {$dest >= 1} {incr dest -1} {
		    conf::add dtnd $id \
			    "route add dtn://host-$dest/* link-$peerid"
		}
	    }
	}
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
proc config_tree_topology {type cl {args ""}} {
    global hosts ports num_nodes id route_to_root

    # the root has routes to all 9 first-hop descendents
    if { $id == 0 } {
	for {set child 1} {$child <= 9} {incr child} {
	    set childaddr $hosts($child)
	    set childport $ports($cl,$child)
	    eval link add link-$child $childaddr:$childport \
		    $type $cl $args
	    
	    route add dtn://host-$child/* link-$child
	}
    }

    # otherwise, the first hop nodes have routes to the root and their
    # 9 second tier descendents
    if { $id >= 1 && $id <= 9 } {
	set parent 0
	set parentaddr $hosts($parent)
	set parentport $ports($cl,$parent)
	eval link add link-$parent $parentaddr:$parentport \
		$type $cl $args
	
	route add dtn://host-$parent/* link-$parent
	set route_to_root dtn://$parentaddr
	
	for {set child [expr $id * 10]} \
		{$child <= [expr ($id * 10) + 9]} \
		{incr child}
	{
	    set childaddr $hosts($child)
	    set childport $ports($cl,$child)
	    eval link add link-$child $childaddr:$childport \
		    $type $cl $args
	    
	    route add dtn://host-$child/* link-$child
	}
    }

    # for third-tier nodes 100-199, set their upstream route. for
    # 100-154, can also set a fourth-tier route to 200-254
    if { $id >= 100 && $id <= 199 } {
	set parent [expr $id / 10]
	set parentaddr $hosts($parent)
	set parentport $ports($cl,$parent)
	eval link add link-$parent $parentaddr:$parentport \
		$type $cl $args
	
	route add dtn://host-$parent/* link-$parent
	set route_to_root dtn://$parentaddr

	if {$id <= 154} {
	    set child [expr $id + 100]
	    set childaddr $hosts($child)
	    set childport $ports($cl,$child)
	    eval link add link-$child $childaddr:$childport \
		    $type $cl $args
	    
	    route add dtn://host-$child/* link-$child
	}
    }

    # finally, the fourth tier nodes 200-254 just set a route to their parent
    if { $id >= 200 && $id <= 255 } {
	set parent [expr $id - 100]
	set parentaddr $hosts($parent)
	set parentport $ports($cl,$parent)
	eval link add link-$parent $parentaddr:$parentport \
		$type $cl $args
	
	route add dtn://host-$parent/* link-$parent
	set route_to_root dtn://$parentaddr
    }
}
