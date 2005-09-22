#
# Basic configuration proc for test dtnd's
#

proc dtn_config {args} {
    # XXX/demmer parse args to make below optional
    
    conf::add dtnd * {source "dtnd-test-utils.tcl"}

    dtn_config_api_server
    dtn_config_berkeleydb
}

#
# Utility proc to get the adjusted port number for various things
#
proc dtn_port {what id} {
    set portbase $net::portbase($id)
    
    switch -- $what {
	console { return [expr $portbase + 0] }
	api	{ return [expr $portbase + 1] }
	tcp	{ return [expr $portbase + 2] }
	udp	{ return [expr $portbase + 2] }
	default { error "unknown port type $what" }
    }
}

#
# Create a new interface for the given convergence layer
#
proc dtn_config_interface {cl args } {
    foreach id [net::nodelist] {
	set host $net::host($id)
	set port [dtn_port $cl $id]
	
	conf::add dtnd $id [eval list interface add ${cl}0 $cl \
		local_addr=$host local_port=$port $args]
    }
}

#
# Set up the API server
#
proc dtn_config_api_server {} {
    foreach id [net::nodelist] {
	conf::add dtnd $id "api set local_addr $net::host($id)"
	conf::add dtnd $id "api set local_port [dtn_port api $id]"
    }
}

#
# Configuure berkeley db storage
#
proc dtn_config_berkeleydb {} {
    conf::add dtnd * {
	storage set type berkeleydb
	storage set payloaddir bundles
	storage set dbname     DTN
	storage set dbdir      db
	storage set dberrlog   DTN.errlog
    }
}

