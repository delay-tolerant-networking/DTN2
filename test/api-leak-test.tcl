test::name api-leak-test
net::num_nodes 1

manifest::file apps/dtntest/dtntest dtntest
manifest::file test/api-leak-test.tcl test-payload.dat

dtn::config

set count 10000
foreach {var val} $opt(opts) {
    if {$var == "-count" || $var == "count"} {
        set count $val
    }
}

test::script {
    puts "* Running dtnd and dtntest"
    dtn::run_dtnd 0
    dtn::run_dtntest 0

    puts "* Waiting for dtnd and dtntest to start up"
    dtn::wait_for_dtnd 0
    dtn::wait_for_dtntest 0

    puts "* Creating / removing $count dtn handles"
    for {set i 0} {$i < $count} {incr i} {
	set id [dtn::tell_dtntest 0 dtn_open]
	dtn::tell_dtntest 0 dtn_close $id
    }

    puts "* Creating one more handle"
    set h [dtn::tell_dtntest 0 dtn_open]

    puts "* Creating / removing $count registrations and bindings"
    for {set i 0} {$i < $count} {incr i} {
	set regid [dtn::tell_dtntest 0 dtn_register $h \
		endpoint=dtn://test expiration=100]
	dtn::tell_dtntest 0 dtn_bind $h $regid
	dtn::tell_dtntest 0 dtn_unbind $h $regid
	dtn::tell_dtntest 0 dtn_unregister $h $regid
    }
    
    puts "* Creating one more registration and binding"
    set regid [dtn::tell_dtntest 0 dtn_register $h \
	    endpoint=dtn://dest expiration=100]
    dtn::tell_dtntest 0 dtn_bind $h $regid
    
    puts "* Sending / receiving $count bundles from memory"
    for {set i 0} {$i < $count} {incr i} {
	dtn::tell_dtntest 0 dtn_send $h source=dtn://source dest=dtn://dest \
		expiration=1 payload_data=this_is_some_test_payload_data
	dtn::tell_dtntest 0 dtn_recv $h payload_mem=true timeout=60
    }
    
    puts "* Sending / receiving $count bundles from files"
    for {set i 0} {$i < $count} {incr i} {
	dtn::tell_dtntest 0 dtn_send $h source=dtn://source dest=dtn://dest \
		expiration=1 payload_file=test-payload.dat
	dtn::tell_dtntest 0 dtn_recv $h payload_file=true timeout=60
    }

    puts "* Checking that all bundles were delivered"
    dtn::wait_for_bundle_stat 0 0 pending
    dtn::wait_for_bundle_stat 0 [expr $count * 2] delivered
    
    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping dtnd and dtntest"
    dtn::stop_dtnd 0
    dtn::stop_dtntest 0
}
