
# Child daemon configurations at this point...
set localhost [info hostname]
set port 7000
# local directory that is scanned for files
set localdir  "/tmp/local-bundles"

set logdir "logs"
set id_file "test/ids"

set fd [open "test/ids" "r+"]
set id [read $fd]
close $fd

incr id

set fd [open $id_file w]
puts $fd $id
close $fd

set fd [open "test/nn" "r+"]
set num_nodes [read $fd]
close $fd

source "test/test-utils.tcl"
source "test/linear-topology.tcl"
source "test/mesh-topology.tcl"
source "test/tree-topology.tcl"

configure_bundle_daemon $id $port $localdir

setup_tree_topology $id $port $num_nodes

test set initscript {

    registration add logger bundles://*/*

    proc test_bundle_arrived {regid bundle_info} {
	puts "A bundle has arrived"
	foreach {source dest payload length} $bundle_info {}
	puts "bundle arrival"
	puts "source:  $source"
	puts "dest:    $dest"
	puts "length:  $length"
	puts "payload: [string range $payload 0 64]"
    }

    set route_table [route dump]

    puts "Route Table for daemon Id: $id\n $route_table"

    tcl_registration bundles://*/* test_bundle_arrived
    
}