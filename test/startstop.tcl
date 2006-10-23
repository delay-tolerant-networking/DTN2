test::name startstop
net::default_num_nodes 1

dtn::config
dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

set clean 0
foreach var $opt(opts) {
    if {$var == "-clean"} {
	set clean 1
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

test::script {
    puts "* Startstop test script executing..."
    dtn::run_dtnd *
    
    puts "* Waiting for dtnd"
    dtn::wait_for_dtnd *
    dtn::tell_dtnd * {puts "startstop test"}

    puts "* Shutting down dtnd"
    dtn::stop_dtnd *

    puts "* Waiting for dtnd to quit"
    run::wait_for_programs

    if {! $clean} {
	puts "* Running new dtnds"
	dtn::run_dtnd *
	
	puts "* Leaving dtn daemons running for the test utils to kill them..."
    }
}
