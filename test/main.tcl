#
# Import the test utilities that we need
#
source "test/dtn-test.tcl"

set id [test set id]

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

route set type "static"

tidy_up

# Create the children who will run with us
create_bundle_daemons $num_nodes

# Set up the the network and routing
setup_${network}_network
setup_${topology}_topology ONDEMAND tcp

