test::name api-leak-test
net::num_nodes 1

manifest::file apps/dtntest/dtntest dtntest

dtn::config

test::script {
    puts "* Running dtnd and dtntest"
    dtn::run_dtnd 0
    dtn::run_dtntest 0

    puts "* Waiting for dtnd and dtntest to start up"
    dtn::wait_for_dtnd 0
    dtn::wait_for_dtntest 0

    set n 10000
    puts "* Creating / removing $n dtn handles"
    for {set i 0} {$i < $n} {incr i} {
	set id [dtn::tell_dtntest 0 dtn_open]
	dtn::tell_dtntest 0 dtn_close $id
    }

    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping dtnd and dtntest"
    dtn::stop_dtnd 0
    dtn::stop_dtntest 0
}
