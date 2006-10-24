#
#    Copyright 2006 Intel Corporation
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

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

    dtn::check_bundle_stats 0 2 pending

    dtn::check_bundle_data 0 bundleid-0 \
	    bundleid     0 \
	    source       $source_eid1 \
	    dest         $dest_eid1   \
	    expiration   20	      \
	    payload_len  [string length $payload]  \
	    payload_data $payload

    dtn::check_bundle_data 0 bundleid-1 \
	    bundleid     1              \
	    source       $source_eid2   \
	    dest         $dest_eid2     \
	    expiration   10		\
	    payload_len  [string length $payload]  \
	    payload_data $payload

    dtn::check test_reg_exists 0 10
    dtn::check test_reg_exists 0 11

    dtn::check_reg_data 0 10   \
	    regid        10        \
	    endpoint     $reg_eid1 \
	    expiration   10        \
	    action       1

    dtn::check_reg_data 0 11   \
	    regid        11        \
	    endpoint     $reg_eid2 \
	    expiration   20        \
	    action       0
}

test::script {
    puts "* starting dtnd..."
    dtn::run_dtnd 0
    
    puts "* waiting for dtnd"
    dtn::wait_for_dtnd 0

    puts "* setting up flamebox ignores"
    tell_dtnd 0 log /test always \
	    "flamebox-ignore ign scheduling IMMEDIATE expiration"

    puts "* injecting two bundles"
    tell_dtnd 0 bundle inject $source_eid1 $dest_eid1 $payload expiration=20
    tell_dtnd 0 bundle inject $source_eid2 $dest_eid2 $payload expiration=10

    puts "* running dtnrecv to create registrations"
    set pid1 [dtn::run_app 0 dtnrecv "-x $reg_eid1 -e 10"]
    set pid2 [dtn::run_app 0 dtnrecv "-x $reg_eid2 -f drop -e 20"]

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

    puts "* shutting down dtnd"
    dtn::stop_dtnd 0

    puts "* waiting for expirations to elapse"
    after 12000

    puts "* restarting dtnd"
    dtn::run_dtnd 0 {}
    dtn::wait_for_dtnd 0

    puts "* checking that 1 bundle expired"
    dtn::check_bundle_stats 0 1 pending

    puts "* checking that 1 registration expired"
    dtn::check ! test_reg_exists 0 10
    dtn::check test_reg_exists 0 11

    puts "* waiting for the others to expire"
    after 10000

    puts "* checking they're expired"
    dtn::check_bundle_stats 0 0 pending
    dtn::check ! test_reg_exists 0 10
    dtn::check ! test_reg_exists 0 11

    puts "* restarting again"
    dtn::stop_dtnd 0
    dtn::run_dtnd 0 {}
    dtn::wait_for_dtnd 0

    puts "making sure they're all really gone"
    dtn::check_bundle_stats 0 0 pending
    dtn::check ! test_reg_exists 0 10
    dtn::check ! test_reg_exists 0 11

    puts "* clearing flamebox ignores"
    tell_dtnd 0 log /test always \
	    "flamebox-ignore ign scheduling IMMEDIATE expiration"
}

test::exit_script {
    puts "* Shutting down dtnd"
    dtn::stop_dtnd 0
}
