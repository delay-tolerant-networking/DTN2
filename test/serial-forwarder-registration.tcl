#
# Tcl based registration that forwards bundles via the mote serial
# source protocol to incoming network chanections.
#

proc serial_forwarder_registration {endpoint port} {

    # create the registration
    set regid [tcl_registration $endpoint ]

    # start the listener socket
    socket -server sfreg_conn_arrived $port
}

proc sfreg_bundle_arrived {regid bundle_data} {
    
    foreach {source dest payload} $bundle_data {}
    set len [string length $payload]
    
    log /sf DEBUG "got bundle of length $len for registration $regid"
    
    set chanVar sfreg_chan_$regid
    global $chanVar

    if {![info exists $chanVar]} {
	log /sf ERROR "bundle arrived with no chanection"
	return
    }

    set chan [set $chanVar]
    puts -nonewline $chan [binary format ba* $len $payload]
}

proc sfreg_conn_arrived {regid chan addr port} {
    set chanVar sfreg_chan_$regid
    global $chanVar

    fconfigure $chan -buffering none
    puts -nonewline $chan "T "

    set check [read $chan 2]
    if {$check != "T "} {
	log /sf ERROR "unexpected value for handshake '$check'"
	close $chan
	return
    }
    
    set $chanVar $chan
}
