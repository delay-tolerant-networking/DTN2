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

test::name many-bundles
net::num_nodes 4
manifest::file apps/dtnrecv/dtnrecv dtnrecv

set clayer       tcp
set count        5000
set length       10
set storage_type berkeleydb

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set clayer $val
    } elseif {$var == "-count" || $var == "count"} {
        set count $val	
    } elseif {$var == "-length" || $var == "length"} {
        set length $val	
    } elseif {$var == "-storage_type" } {
	set storage_type $val
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

puts "* Configuring $clayer interfaces / links"
dtn::config -storage_type $storage_type
dtn::config_interface $clayer
dtn::config_linear_topology OPPORTUNISTIC $clayer true

test::script {
    set starttime [clock seconds]
    
    puts "* Running dtnd 0"
    dtn::run_dtnd 0
    dtn::wait_for_dtnd 0

    set source    dtn://host-0/test
    set dest      dtn://host-3/test
    
    puts "* Sending $count bundles of length $length"
    for {set i 0} {$i < $count} {incr i} {
	set timestamp($i) [dtn::tell_dtnd 0 sendbundle $source $dest\
		length=$length expiration=3600]
    }

    puts "* Checking that all the bundles are queued"
    dtn::wait_for_bundle_stats 0 "$count pending"

    puts "* Restarting dtnd 0"
    dtn::stop_dtnd 0
    after 5000
    dtn::run_dtnd 0 ""

    puts "* Checking that all bundles were re-read from the database"
    dtn::wait_for_dtnd 0
    dtn::wait_for_bundle_stats 0 "$count pending"

    puts "* Starting dtnd 1"
    dtn::run_dtnd 1
    dtn::wait_for_dtnd 1

    puts "* Opening link to dtnd 1"
    dtn::tell_dtnd 0 link open $clayer-link:0-1

    puts "* Waiting for all bundles to flow (up to 5 minutes)"
    set timeout [expr 5 * 60 * 1000]
    
    dtn::wait_for_bundle_stats 0 "0 pending"       $timeout
    dtn::wait_for_bundle_stats 1 "$count pending"  $timeout

    puts "* Checking that the link stayed open"
    dtn::check_link_stats 0 $clayer-link:0-1 "1 contacts"
    dtn::check_link_stats 1 $clayer-link:1-0 "1 contacts"

    puts "* Starting dtnd 2 and 3"
    dtn::run_dtnd 2
    dtn::run_dtnd 3
    dtn::wait_for_dtnd 2
    dtn::wait_for_dtnd 3

    puts "* Starting dtnrecv on node 3"
    set rcvpid [dtn::run_app 3 dtnrecv "-q -n $count $dest"]

    puts "* Opening links"
    dtn::tell_dtnd 2 link open $clayer-link:2-3
    dtn::tell_dtnd 1 link open $clayer-link:1-2

    puts "* Waiting for all bundles to be delivered"
    dtn::wait_for_bundle_stats 1 "0 pending"        $timeout
    dtn::wait_for_bundle_stats 2 "0 pending"        $timeout
    dtn::wait_for_bundle_stats 3 "$count delivered" $timeout

    puts "* Checking that all links stayed open"
    dtn::check_link_stats 0 $clayer-link:0-1 "1 contacts"
    dtn::check_link_stats 1 $clayer-link:1-0 "1 contacts"
    dtn::check_link_stats 1 $clayer-link:1-2 "1 contacts"
    dtn::check_link_stats 2 $clayer-link:2-1 "1 contacts"
    dtn::check_link_stats 2 $clayer-link:2-3 "1 contacts"
    dtn::check_link_stats 3 $clayer-link:3-2 "1 contacts"

    puts "* Waiting for receiver to complete"
    run::wait_for_pid_exit 3 $rcvpid

    puts "* Test success!"

    set elapsed [expr [clock seconds] - $starttime]
    puts "* TEST TIME: $elapsed seconds"

}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
