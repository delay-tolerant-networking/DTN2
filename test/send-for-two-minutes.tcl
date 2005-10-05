test::name send-for-two-minutes
net::num_nodes 3

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

conf::add dtnd 0 {
    
    proc test_arrived {regid bundle_data} {
        array set b $bundle_data
        if ($b(isadmin)) {
            error "Unexpected admin bundle arrival $b(source) -> b($dest)"
        }
        puts "bundle arrival"
        foreach {key val} [array get b] {
            if {$key == "payload"} {
                puts "payload:\t [string range $b(payload) 0 64]"
            } else {
                puts "$key:\t $b($key)"
            }
        }
        # record the bundle's arrival
        set guid "$b(source),$b(creation_ts)"
        global bundle_payloads
        set bundle_payloads($guid) $b(payload)
        global bundle_info
        unset b(payload)
        set bundle_info($guid) [array get b]
    }

    proc check_bundle_arrived {bundle_guid} {
        global bundle_info
        if {[info exists bundle_info($bundle_guid)]} {
            return true
        }
        return false
    }
    
}

test::script {
    puts "* running dtnds"
    dtn::run_dtnd *

    puts "* waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    set last_node [expr [net::num_nodes] - 1]
    set source [dtn::tell_dtnd $last_node {
        route local_eid
    }]
    
    dtn::tell_dtnd 0 {
        tcl_registration dtn://host-0/test test_arrived
    }

    puts "* sending one bundle every 10 seconds for 2 minutes"
    set timestamps {}
    for {set i 1} {$i <= 13} {incr i} {
        puts "* sending bundle $i"
        lappend timestamps [dtn::tell_dtnd $last_node {
            sendbundle [route local_eid] dtn://host-0/test
        }]
        # save 10 seconds at the end of the test:
        if {$i < 13} {
            after 10000
        }
    }
    
    set i 1
    foreach timestamp $timestamps {
        puts -nonewline "* Checking that bundle $i arrived... "
        do_until "checking that bundle $i arrived" 5000 [subst -nocommand {
                    if {[dtn::tell_dtnd 0 {check_bundle_arrived \
                                               "$source,$timestamp"}]} {
                        break
                    }
        }]
        # the Marv Albert of test reports:
        puts "yes! And it counts!"
        incr i
    }
    puts "* test success!"
}

test::exit_script {
    puts "* stopping all dtnds"
    dtn::tell_dtnd * exit
}
