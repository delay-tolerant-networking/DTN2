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
    set hosts{2} ica
}

