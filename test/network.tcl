#
# Procs for setting up the test network. Each proc sets the global hosts
# array indexed by id
#

proc setup_loopback_network {{base 10.0.0}} {
    global hosts
    
    for {set i 1} {$i < 255} {incr i} {
	set hosts([expr $i - 1]) $base.$i
    }
}

proc setup_intel_network {} {
    global hosts

    set hosts{0} sandbox
    set hosts{1} ica
    # XXX/demmer fix me
}

proc setup_interface {cl {port 5000}} {
    global hosts id
    route local_tuple bundles://internet/host://$hosts($id)
    interface add $cl host://$hosts($id):$port
    api set local_addr $hosts($id)
}

