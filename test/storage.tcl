test::name storage
net::default_num_nodes 1

manifest::file apps/dtnsend/dtnsend dtnsend
manifest::file apps/dtnrecv/dtnrecv dtnrecv

set storage_type berkeleydb
foreach {var val} $opt(opts) {
    if {$var == "-storage_type" || $var == "storage_type"} {
	set storage_type $val
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

dtn::config -storage_type $storage_type

proc rand_int {} {
    return [expr int(rand() * 1000)]
}

set source_eid1 dtn://storage-test-src-[rand_int]
set source_eid2 dtn://storage-test-src-[rand_int]
set dest_eid1   dtn://storage-test-dst-[rand_int]
set dest_eid2   dtn://storage-test-dst-[rand_int]
set reg_eid1    dtn://storage-test-reg-[rand_int]
set reg_eid2    dtn://storage-test-reg-[rand_int]

set payload    "storage test payload"

proc check {} {
    global source_eid1 source_eid2
    global dest_eid1 dest_eid2
    global reg_eid1 reg_eid2
    global payload
    
    dtn::check_bundle_data 0 bundleid-0 \
	    bundleid     0 \
	    source       $source_eid1 \
	    dest         $dest_eid1   \
	    payload_len  [string length $payload]  \
	    payload_data $payload

    dtn::check_bundle_data 0 bundleid-1 \
	    bundleid     1              \
	    source       $source_eid2   \
	    dest         $dest_eid2     \
	    payload_len  [string length $payload]  \
	    payload_data $payload

    dtn::check_reg_data 0 10   \
	    regid        10        \
	    endpoint     $reg_eid1 \
	    expiration   20        \
	    action       1

    dtn::check_reg_data 0 11   \
	    regid        11        \
	    endpoint     $reg_eid2 \
	    expiration   30        \
	    action       0
}

test::script {
    puts "* starting dtnd..."
    dtn::run_dtnd 0
    
    puts "* waiting for dtnd"
    dtn::wait_for_dtnd 0

    puts "* injecting two bundles"
    tell_dtnd 0 bundle inject $source_eid1 $dest_eid1 $payload
    tell_dtnd 0 bundle inject $source_eid2 $dest_eid2 $payload

    puts "* running dtnrecv to create registrations"
    set pid1 [dtn::run_app 0 dtnrecv "-x $reg_eid1 -e 20"]
    set pid2 [dtn::run_app 0 dtnrecv "-x $reg_eid2 -f drop -e 30"]

    run::wait_for_pid_exit 0 $pid1
    run::wait_for_pid_exit 0 $pid2

    puts "* checking the data before shutdown"
    check

    puts "* restarting dtnd without the tidy option"
    dtn::stop_dtnd 0
    dtn::run_dtnd 0 {}
    dtn::wait_for_dtnd 0

    puts "* checking the data after reloading the database"
    check
}

test::exit_script {
    puts "* Shutting down dtnd"
    dtn::stop_dtnd 0
}
