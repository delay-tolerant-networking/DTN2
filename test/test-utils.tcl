#
# Testing utilities: create daemons, cleaning, etc
#
  
#
# Forks off num_nodes total child daemons
#
proc create_bundle_daemons { num_nodes } {
    set id [test set id]

    log prefix "$id: "
    
    if {$id != 0} {
	return
    }
    
    for {set childid 1} {$childid < $num_nodes} {incr childid} {
	set log_path "logs/daemon.$childid"

	set argv [test set argv]
	lappend argv -i $childid
	lappend argv -d
	lappend argv < /dev/null

	if {[log set logfile] != "-"} {
	    lappend argv -o $log_path
	}
	
	puts "booting daemon: $argv"
	eval exec $argv &
    }
}

#
# DB configuration
#
proc config_db {{type berkeleydb}} {
    global env
    
    set id [test set id]

    storage set tidy      true
    storage set type	  $type
    storage set sqldb	  "dtn$id"
    storage set dbdir	  "/tmp/bundledb-$env(USER)-$id"
    storage set payloaddir "/tmp/bundles-$env(USER)-$id"
}

#
# Tidy implementation (should move to C)
#
proc tidy_up {} {
    set tidy    	[storage set tidy]
    set dbdir		[storage set dbdir]
    set payloaddir	[param set payload_dir]
    set id 		[test set id]
    
    if {$id != 0} {
	foreach dir [list $dbdir $payloaddir] {
	    if {! [file exists $dir]} {
		file mkdir $dir
	    }
	}
    }
    
    # clean up 
    if {$tidy} {
	log /tidy INFO "tidy option set, cleaning payload and local file dirs"
	foreach dir [list $dbdir $payloaddir] {
	    file delete -force $dir
	    file mkdir $dir
	}
    }
    
    # validate the directories (XXX/demmer move this into C eventually)
    foreach dirtype [list dbdir payloaddir] {
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
# test proc for sending a bundle
#
proc sendbundle {{peerid -1} {demux "test"}} {
    global hosts id

    #
    # Special case hook for id 0 and 1 pairing
    #
    if {$peerid == -1} {
	if {$id == 0} {
	    set peerid 1
	} elseif {$id == 1} {
	    set peerid 0
	} else {
	    error "must specify peer id"
	}
    } 
    
    set length  5000
    set payload "test bundle payload data\n"

    while {$length - [string length $payload] > 32} {
	append payload [format "%4d: 0123456789abcdef\n" [string length $payload]]
    }
    while {$length > [string length $payload]} {
	append payload "."
    }
    
    bundle inject bundles://internet/host://$hosts($id)/		\
	    	  bundles://internet/host://$hosts($peerid)/test	\
		  $payload $length
}

