#
# Testing utilities: create daemons, cleaning, etc
#
  
#
# Forks off num_nodes total child daemons
#
proc create_bundle_daemons { num_nodes } {
    set id [test set id]
    
    if {$id != 0} {
	return
    }
    
    for {set childid 1} {$childid < $num_nodes} {incr childid} {
	set log_path "logs/daemon.$childid"

	set argv [test set argv]
	lappend argv -i $childid
	lappend argv -d
	lappend argv < /dev/null

	if {! [test set log_to_stdout]} {
	    lappend argv -o $log_path
	}
	
	puts "booting daemon: $argv"
	eval exec $argv &
    }
}

#
# Tidy implementation (should move to C)
#
proc tidy_up {} {
    set tidy    [storage set tidy]
    set id [test set id]
    
    if {$id != 0} {
	foreach dir [list $dbdir $payloaddir $localdir] {
	    if {! [file exists $dir]} {
		file mkdir $dir
	    }
	}
    }
    
    # clean up 
    if {$tidy} {
	log /tidy INFO "tidy option set, cleaning payload and local file dirs"
	foreach dir [list $dbdir $payloaddir $localdir ] {
	    file delete -force $dir
	    file mkdir $dir
	}
    }
    
    # validate the directories (XXX/demmer move this into C eventually)
    foreach dirtype [list dbdir payloaddir localdir] {
	set dir [set $dirtype]
	if {! [file exists $dir]} {
	    error "$dirtype directory $dir doesn't exist"
	}
	
	if {[file type $dir] != "directory"} {
	    error "$dirtype directory $dir is not a directory"
	}
	
	if {[glob -nocomplain $dir/*] != {}} {
	    error "$dirtype directory $dir not empty and re-reading " \
		    "state not implemented (use the -t option)"
	}
    }
}

#
# Setup the configuration for the bundle daemon
#
proc configure_bundle_daemon { id port localdir } {
    
    # Initialize local configurations
    set port [expr $port + $id]
    set sqldb "dtn$id"
    set dbdir "/tmp/bundledb-$id"
    set payloaddir  "/tmp/bundles-$id"
    set localhost [info hostname]
    set peer $localhost
        
    # initialize storage
    storage set sqldb $sqldb
    storage set dbdir $dbdir
    
    # make this "storage type"
    storage init berkeleydb
    
    # set the payload directory and other params
    param set payload_dir $payloaddir
    param set tcpcl_ack_blocksz 4096
    
    # param set payload_test_no_remove true
    # param set udpcl_test_fragment_size 500
    #param set payload_mem_threshold 0
    param set proactive_frag_threshold 495
    
    # set a local tcp interface
    set tcp_local_tuple bundles://internet/tcp://$localhost:$port/
    interface $tcp_local_tuple
    
    # set a local udp interface
    set udp_local_tuple bundles://internet/udp://$localhost:$port/
    interface $udp_local_tuple
    
    # and a file one
    set file_local_tuple file://unix/file://$localdir/
    interface $file_local_tuple
    
}

proc static {args} {
    set procName [lindex [info level [expr [info level]-1]] 0]
    foreach varName $args {
	uplevel 1 "upvar #0 {$procName:$varName} $varName"
    }
}
