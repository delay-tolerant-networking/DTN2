test::name bundle-status-reports
net::num_nodes 3

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

test::script {

    proc test_sr {sr_option sr_field node_list} {

	global last_node
	global source
	global sr_dest
	global dest
	global dest_node
	
	puts "* Sending bundle to $dest with $sr_option set"
	# 3 second expiration for deleted SR's to be generated in time:
	set timestamp [dtn::tell_dtnd $last_node \
			   sendbundle $source $dest replyto=$sr_dest \
			   expiration=3 $sr_option]

	# If we selected deletion receipt we assume the bundle
	# won't ever arrive at the destination
	if {$sr_option != "deletion_rcpt"} {
	    puts "* Waiting for bundle arrival at $dest"
	    dtn::wait_for_bundle 0 "$source,$timestamp" 5000
	}
    
	set sr_guid "$source,$timestamp,$dest_node"

	puts "* Testing for $sr_option SR(s):"
	foreach node_name $node_list {
	    set sr_guid "$source,$timestamp,$node_name"

	    puts "  * Waiting for SR arrival from $node_name to $sr_dest"
	    dtn::wait_for_sr $last_node $sr_guid 5000
	    
	    puts "  * Verifying received SR has field \"$sr_field\""
	    dtn::check_sr_fields $last_node $sr_guid $sr_field

	}
    }

    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    # global constants
    set last_node [expr [net::num_nodes] - 1]
    set source    [dtn::tell_dtnd $last_node {route local_eid}]
    set dest_node dtn://host-0
    set dest      $dest_node/test
    set sr_dest   $source/sr
    set all_nodes [list]
    for {set i 0} {$i <= $last_node} {incr i} {
	lappend all_nodes "dtn://host-$i"
    }

    # report bundle arrivals at the bundle delivery points
    dtn::tell_dtnd $last_node tcl_registration $sr_dest
    dtn::tell_dtnd 0 tcl_registration $dest

    # test the SR's
    test_sr delivery_rcpt delivered_time $dest_node
    test_sr forward_rcpt forwarded_time $all_nodes
    test_sr receive_rcpt received_time $all_nodes
    
    # need to cut the link to create a deletion SR:
    dtn::tell_dtnd 1 link close link-0
    after 1000
    test_sr deletion_rcpt deleted_time dtn://host-1

    puts "* Success!"
    
}

test::exit_script {
    puts "* stopping all dtnds"
    dtn::tell_dtnd * exit
}
