#
# Import the test utilities that we need
#
source "test/dtn-test.tcl"

set id [test set id]

# Make sure the test script set the number of nodes
if {! [info exists num_nodes]} {
    log /test WARNING "test script should set num_nodes, using default two"
    set num_nodes 2
}

# Create the children who will run with us
create_bundle_daemons $num_nodes

# # Configure ourself
# configure_bundle_daemon $id $port $localdir

# # Setup our routing to peers (make selection here)
# setup_tree_topology $id $port $num_nodes
