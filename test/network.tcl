#
# Procs for setting up the test network. Each proc sets the global hosts
# array indexed by id and the global ports array indexed by proto,id.
#

#
# For running a bunch of tests on loopback, we use ports 10000-20000,
# divided up into 10 port chunks for each host.
#
# So, host 0 would run the api on port 10000, tcp on port 10001, etc,
# while host 52 would run the api on 15200, tcp on 15201, etc.
#
proc setup_loopback_network {} {
    global hosts ports
    
    for {set i 0} {$i < 100} {incr i} {
	set hosts($i) localhost

	set ports(api,$i) [expr 10000 + ($i * 100) + 0]
	set ports(tcp,$i) [expr 10000 + ($i * 100) + 1]
	set ports(udp,$i) [expr 10000 + ($i * 100) + 2]
    }
    
}

#
# Network to run on separate loopback alias network devices
#
proc setup_loopback_alias_network {{base 10.0.0}} {
    global hosts ports
    
    for {set i 1} {$i < 100} {incr i} {
	set hosts([expr $i - 1]) $base.$i

	set ports(api,$i) 5010
	set ports(tcp,$i) 5000
	set ports(udp,$i) 5001
    }
}

#
# Test network definition inside intel
#
proc setup_intel_network {} {
    global hosts ports

    # XXX/demmer fix me
    set hosts{0} sandbox
    set hosts{1} ica

    for {set i 1} {$i < 255} {incr i} {
	set ports(api,$i) 5010
	set ports(tcp,$i) 5000
	set ports(udp,$i) 5000
    }
}
