#
# Utility proc to get the adjusted port number for various things
#
proc dtn_port {what id} {
    set portbase $net::portbase($id)
    
    switch -- $what {
	api	{ return [expr $portbase + 0] }
	tcp	{ return [expr $portbase + 1] }
	udp	{ return [expr $portbase + 1] }
	default { error "unknown port type $what" }
    }
}

#
# Create a new interface for the given convergence layer
#
proc config_interface {cl args } {
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
proc config_api_server {} {
    foreach id [net::nodelist] {
	conf::add dtnd $id api set local_addr $net::host($id)
	conf::add dtnd $id api set local_port [dtn_port api $id]
    }
}

#
# Configuure berkeley db storage
#
proc config_berkeleydb {} {
    conf::add dtnd * {
	storage set type berkeleydb
	storage set payloaddir bundles
	storage set dbname     DTN
	storage set dbdir      db
	storage set dberrlog   DTN.errlog
    }
}
