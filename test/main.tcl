
set id [test set id]
set num_nodes 6
set localhost [info hostname]
set port 7000
# local directory that is scanned for files
set localdir  "/tmp/local-bundles" 

#
# Import the test utilities that we need
#
source "test/test-utils.tcl"
source "test/linear-topology.tcl"
source "test/mesh-topology.tcl"
source "test/tree-topology.tcl"

# Clean all the previous logs...
clean_logs $num_nodes

# Create the children who will run with us
create_bundle_daemons $num_nodes

# Configure ourself
configure_bundle_daemon $id $port $localdir

# Setup our routing to peers (make selection here)
setup_tree_topology $id $port $num_nodes

#
# Now execute whatever you want in the testscript
#
test set initscript {
    registration add logger bundles://*/*

    set peer $localhost
    
    proc test_bundle_arrived {regid bundle_info} {
	foreach {source dest payload length} $bundle_info {}
	puts "bundle arrival"
	puts "source:  $source"
	puts "dest:    $dest"
	puts "length:  $length"
	puts "payload: [string range $payload 0 64]"
    }

#    tcl_registration bundles://*/* test_bundle_arrived

    file_injector_start $localdir bundles://internet/bundles://$localhost/ \
	    bundles://internet/bundles://$peer/demux

}
