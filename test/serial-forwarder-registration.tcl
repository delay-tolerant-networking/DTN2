#
# Tcl based registration that forwards bundles via the mote serial
# source protocol to incoming network chanections.
#

proc serial_forwarder_registration {endpoint port} {

    # create the registration
    set regid [tcl_registration $endpoint sfreg_bundle_arrived]

    # start the listener socket
    socket -server "sfreg_conn_arrived $regid" $port
}

proc sfreg_bundle_arrived {regid bundle_data} {

    array set b $bundle_data
    
    if ($b(isadmin)) {
	error "Unexpected admin bundle arrival $b(source) -> $b(dest)"
    }
    
    log /sf debug "got bundle of length $b(length) for registration $regid"
    
    set chanVar sfreg_chan_$regid
    global $chanVar

    if {![info exists $chanVar]} {
	log /sf ERROR "bundle arrived with no connection"
	return
    }

    set chan [set $chanVar]
    puts -nonewline $chan [binary format ca* $b(length) $b(payload)]
    flush $chan

}

proc sfreg_conn_arrived {regid chan addr port} {
    set chanVar sfreg_chan_$regid
    global $chanVar

    log /sf debug "got new serial forwarder connection for registration $regid"

    fconfigure $chan -buffering none
    fconfigure $chan -translation binary
    puts -nonewline $chan "T "
    flush $chan

    set check [read $chan 2]
    if {$check != "T "} {
	log /sf ERROR "unexpected value for handshake '$check'"
	close $chan
	return
    }
    
    set $chanVar $chan

    log /sf debug "handshake complete"
}
