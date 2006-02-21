#
# Default config shared by all these topologies
#

namespace eval dtn {

proc config_topology_common {} {
    foreach id [net::nodelist] {
	conf::add dtnd $id "route set type static"
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
# Set up a linear topology using TCP or UDP
#
proc config_linear_topology {type cl with_routes {args ""}} {
    dtn::config_topology_common
    
    set last [expr [net::num_nodes] - 1]
    foreach id [net::nodelist] {
	# link to next hop in chain (except for last one)
	if { $id != $last } {
	    set peerid   [expr $id + 1]
	    set peeraddr $net::host($peerid)
	    set peerport [dtn::get_port $cl $peerid]
	    set link [get_link_name $cl $id $peerid]

	    conf::add dtnd $id [eval list link add $link  \
		    $peeraddr:$peerport $type $cl $args]

	    if {$with_routes} {
		for {set dest $peerid} {$dest <= $last} {incr dest} {
		    conf::add dtnd $id \
			    "route add dtn://host-$dest/* $link"
		}
	    }
	}
	
	# and previous a hop in chain as well
	if { $id != 0 } {
	    set peerid   [expr $id - 1]
	    set peeraddr $net::host($peerid)
	    set peerport [dtn::get_port $cl $peerid]

	    set link [get_link_name $cl $id $peerid]
	    conf::add dtnd $id [eval list link add $link \
		    $peeraddr:$peerport  $type $cl $args]
	    
	    if {$with_routes} {
		for {set dest $peerid} {$dest >= 0} {incr dest -1} {
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
proc config_tree_topology {type cl {args ""}} {
    global hosts ports num_nodes id route_to_root

    error "XXX/demmer fixme"

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

# A two level tree with a single root and children
#            0
#      /  / ...  \  \
#     1     ...      n
#
proc config_twolevel_topology {type cl with_routes {args ""}} {   
    # setup ingress links for each node
    set root_addr $net::host(0)
    set root_port [dtn::get_port $cl 0]

    foreach id [net::nodelist] {
	set to_root_link   [get_link_name $cl $id 0]
	set from_root_link [get_link_name $cl 0 $id]

	if { $id != 0 } {
	    # setup links b/t root and children and routes
	    set child_addr $net::host($id)
	    set child_port [dtn::get_port $cl $id]
	    
	    conf::add dtnd 0 [eval list link add $from_root_link \
				  $child_addr:$child_port $type $cl $args]
	    conf::add dtnd $id [eval list link add $to_root_link \
				    $root_addr:$root_port $type $cl $args]

	    if {$with_routes} {
		for {set child 0} {$child < [net::num_nodes]} {incr child} {
		    if {$child != $id} {
			conf::add dtnd $id \
			    "route add dtn://host-$child/* $to_root_link"
		    }
		}
		conf::add dtnd 0 "route add dtn://host-$id/* $from_root_link"
	    }
	}
    }
}

# namespace dtn
}
