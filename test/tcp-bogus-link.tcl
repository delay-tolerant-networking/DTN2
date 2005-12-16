test::name tcp-bogus-link
net::num_nodes 1

dtn::config

test::script {
    puts "* Running dtnd"
    dtn::run_dtnd 0

    puts "* Waiting for dtnd to start up"
    dtn::wait_for_dtnd 0

    puts "* Adding a bogus link"
    dtn::tell_dtnd 0 link add bogus 1.2.3.4:9999 ALWAYSON tcp

    puts "* Checking link is OPENING"
    dtn::check_link_state 0 bogus OPENING

    puts "* Closing bogus link"
    dtn::tell_dtnd 0 link close bogus

    puts "* Checking link is UNAVAILABLE"
    dtn::check_link_state 0 bogus UNAVAILABLE

    puts "* Reopening link"
    dtn::tell_dtnd 0 link open bogus
    
    puts "* Checking link is OPENING"
    dtn::check_link_state 0 bogus OPENING
    
    puts "* Closing bogus link again"
    dtn::tell_dtnd 0 link close bogus

    puts "* Checking link is UNAVAILABLE"
    dtn::check_link_state 0 bogus UNAVAILABLE
    
    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
