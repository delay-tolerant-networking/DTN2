#
# Basic configuration proc for test dtnd's
#

namespace eval dtn {

proc config {args} {
    # XXX/demmer parse args to make below optional
    
    conf::add dtnd * {source "dtnd-test-utils.tcl"}

    dtn::standard_manifest
    dtn::config_console
    dtn::config_api_server
    dtn::config_berkeleydb
}

#
# Standard manifest
#
proc standard_manifest {} {
    manifest::file daemon/dtnd dtnd
    manifest::file test-utils/dtnd-test-utils.tcl dtnd-test-utils.tcl
    manifest::dir  bundles
    manifest::dir  db
}

#
# Utility proc to get the adjusted port number for various things
#
proc get_port {what id} {
    global net::portbase
    set portbase $net::portbase($id)
    
    switch -- $what {
	console { return [expr $portbase + 0] }
	api	{ return [expr $portbase + 1] }
	tcp	{ return [expr $portbase + 2] }
	udp	{ return [expr $portbase + 2] }
	default { return -1 }
    }
}

#
# Create a new interface for the given convergence layer
#
proc config_interface {cl args } {
    foreach id [net::nodelist] {
	set host $net::host($id)
	set port [dtn::get_port $cl $id]
	
	conf::add dtnd $id [eval list interface add ${cl}0 $cl \
		local_addr=$host local_port=$port $args]
    }
}

#
# Configure the console server
#
proc config_console {} {
    foreach id [net::nodelist] {
	conf::add dtnd $id "console set addr $net::host($id)"
	conf::add dtnd $id "console set port [dtn::get_port console $id]"
    }
}

#
# Set up the API server
#
proc config_api_server {} {
    foreach id [net::nodelist] {
	conf::add dtnd $id "api set local_addr $net::host($id)"
	conf::add dtnd $id "api set local_port [dtn::get_port api $id]"
    }
}

#
# Configuure berkeley db storage
#
proc config_berkeleydb {} {
    conf::add dtnd * {
	storage set tidy_wait  0
	storage set type       berkeleydb
	storage set payloaddir bundles
	storage set dbname     DTN
	storage set dbdir      db
	storage set dberrlog   DTN.errlog
    }
}

# namespace dtn
}
