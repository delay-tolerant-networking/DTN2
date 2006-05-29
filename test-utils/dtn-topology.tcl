#
# Default config shared by all these topologies
#

namespace eval dtn {

set router_type "static"

proc config_topology_common {} {
    global dtn::router_type
    
    foreach id [net::nodelist] {
	conf::add dtnd $id "route set type $router_type"
	conf::add dtnd $id "route local_eid dtn://host-$id"
    }
}

#
# Generate a unique link name
#
proc get_link_name {cl src dst} {
    global dtn::link_names

    set name "$cl-link:$src-$dst"
    if {![info exists link_names($name)] } {
	set link_names($name) 1
	return $name
    }

    set i 2
    while {1} {
	set name "$cl-link.$i:$src-$dst"
	if {![info exists link_names($name)]} {
	    set link_names($name) 1
	    return $name
	}
	incr i
    }
}

#
# Add a link and optionally a route from a to b
#
proc config_link {id peerid type cl with_routes link_args} {
    set peeraddr $net::host($peerid)
    set peerport [dtn::get_port $cl $peerid]
    set link [get_link_name $cl $id $peerid]
    
    conf::add dtnd $id [join [list \
	    link add $link $peeraddr:$peerport $type $cl $link_args]]
    
    if {$with_routes} {
	# XXX/demmer this should become unnecessary since the one-hop
	# neigbors should be automatically added to the route table
	conf::add dtnd $id \
		"route add dtn://host-$peerid/* $link"
    }
}

#
# Set up a linear topology using TCP or UDP
#
proc config_linear_topology {type cl with_routes {link_args ""}} {
    dtn::config_topology_common
    
    set last [expr [net::num_nodes] - 1]
    
    foreach id [net::nodelist] {

	# link to next hop in chain (except for last one) and routes
	# to non-immediate neighbors
	if { $id != $last} {
	    config_link $id [expr $id + 1] $type $cl $with_routes $link_args

	    for {set dest [expr $id + 2]} {$dest <= $last} {incr dest} {
		conf::add dtnd $id \
			"route add dtn://host-$peerid/* $link"
	    }
	}
	
	# and previous a hop in chain as well
	if { $id != 0 } {
	    config_link $id [expr $id - 1] $type $cl $with_routes $link_args

	    if {$with_routes} {
		for {set dest [expr $id - 2]} {$dest >= 0} {incr dest -1} {
		    conf::add dtnd $id \
			    "route add dtn://host-$dest/* $link"
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
proc config_tree_topology {type cl {link_args ""}} {
    global hosts ports num_nodes id route_to_root

    error "XXX/demmer fixme"

    # the root has routes to all 9 first-hop descendents
    if { $id == 0 } {
	for {set child 1} {$child <= 9} {incr child} {
	    set childaddr $hosts($child)
	    set childport $ports($cl,$child)
	    eval link add link-$child $childaddr:$childport \
		    $type $cl $link_args
	    
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
		$type $cl $link_args
	
	route add dtn://host-$parent/* link-$parent
	set route_to_root dtn://$parentaddr
	
	for {set child [expr $id * 10]} \
		{$child <= [expr ($id * 10) + 9]} \
		{incr child}
	{
	    set childaddr $hosts($child)
	    set childport $ports($cl,$child)
	    eval link add link-$child $childaddr:$childport \
		    $type $cl $link_args
	    
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
		$type $cl $link_args
	
	route add dtn://host-$parent/* link-$parent
	set route_to_root dtn://$parentaddr

	if {$id <= 154} {
	    set child [expr $id + 100]
	    set childaddr $hosts($child)
	    set childport $ports($cl,$child)
	    eval link add link-$child $childaddr:$childport \
		    $type $cl $link_args
	    
	    route add dtn://host-$child/* link-$child
	}
    }

    # finally, the fourth tier nodes 200-254 just set a route to their parent
    if { $id >= 200 && $id <= 255 } {
	set parent [expr $id - 100]
	set parentaddr $hosts($parent)
	set parentport $ports($cl,$parent)
	eval link add link-$parent $parentaddr:$parentport \
		$type $cl $link_args
	
	route add dtn://host-$parent/* link-$parent
	set route_to_root dtn://$parentaddr
    }
}

# A two level tree with a single root and N children
#            0
#      /  / ...  \  \
#     1     ...      n
#
proc config_twolevel_topology {type cl with_routes {link_args ""}} {
    foreach id [net::nodelist] {
	if { $id != 0 } {
	    config_link 0   $id $type $cl $with_routes $link_args
	    config_link $id 0   $type $cl $with_routes $link_args
	}
    }
}

# A simple four node diamond topology with multiple
# routes of the same priority for two-hop neighbors.
#
#      0
#     / \
#    1   2
#     \ /
#      3
proc config_diamond_topology {type cl with_routes {link_args ""}} {
    dtn::config_topology_common
    
    config_link 0 1 $type $cl $with_routes $link_args
    config_link 0 2 $type $cl $with_routes $link_args
    
    conf::add dtnd 0 "route add dtn://host-3/* $cl-link:0-1"
    conf::add dtnd 0 "route add dtn://host-3/* $cl-link:0-2"
    
    config_link 1 0 $type $cl $with_routes $link_args
    config_link 1 3 $type $cl $with_routes $link_args

    conf::add dtnd 1 "route add dtn://host-2/* $cl-link:1-0"
    conf::add dtnd 1 "route add dtn://host-2/* $cl-link:1-3"
    
    config_link 2 0 $type $cl $with_routes $link_args
    config_link 2 3 $type $cl $with_routes $link_args

    conf::add dtnd 2 "route add dtn://host-1/* $cl-link:2-0"
    conf::add dtnd 2 "route add dtn://host-1/* $cl-link:2-3"
    
    config_link 3 1 $type $cl $with_routes $link_args
    config_link 3 2 $type $cl $with_routes $link_args

    conf::add dtnd 3 "route add dtn://host-0/* $cl-link:3-1"
    conf::add dtnd 3 "route add dtn://host-0/* $cl-link:3-2"
}

# namespace dtn
}
