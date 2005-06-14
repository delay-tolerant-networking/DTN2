#
# Import the test utilities that we need
#
source "test/dtn-test.tcl"

set id [test set id]
log prefix "$id:"

# Make sure the test script set the requisite variables
if {! [info exists num_nodes]} {
    log /test WARNING "test script should set num_nodes, using default two"
    set num_nodes 2
}

if {! [info exists network]} {
    log /test WARNING "test script should set network, using default loopback"
    set network loopback
}

if {! [info exists topology]} {
    log /test WARNING "test script should set topology, using default linear"
    set topology linear
}

# default to static routing
route set type "static"

# configure the database (and tidy up)
config_db

# create the children who will run with us
if {[test set fork]} {
    create_bundle_daemons $num_nodes
}

# set up the the network
if {$network != "none"} {
    setup_${network}_network
}

# set up the api server
if {! [info exists no_api] } {
    if {! [info exists ports(api,$id)]} {
	error "no api ports entry for id $id"
    }

    api set local_addr $hosts($id)
    api set local_port $ports(api,$id)
}

# set up the local route tupl
if {! [info exists no_local_route]} {
    route local_tuple bundles://internet/host://host-$id
}

#
# Proc to create a new interface for the given convergence layer
#
proc setup_interface {cl {args ""}} {
    global hosts ports id

    if {! [info exists ports($cl,$id)]} {
	error "no ports entry for cl $cl id $id"
    }

    set port $ports($cl,$id)
    
    eval interface add $cl host://$hosts($id):$port $args
}

