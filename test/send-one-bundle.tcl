#
#    Copyright 2004-2006 Intel Corporation
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

test::name send-one-bundle
net::default_num_nodes 2

dtn::config

set clayer tcp
set length 5000
set segmentlen 0

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set clayer $val
    } elseif {$var == "-length" || $var == "length"} {
        set length $val	
    } elseif {$var == "-segmentlen" || $var == "segmentlen"} {
        set segmentlen $val	
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

puts "* Configuring $clayer interfaces / links"
dtn::config_interface $clayer

set linkopts ""
if {$segmentlen != 0} {
    set linkopts "segment_length=$segmentlen"
}
    
dtn::config_linear_topology ALWAYSON $clayer true $linkopts

test::script {
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    set last_node [expr [net::num_nodes] - 1]
    set source    [dtn::tell_dtnd $last_node {route local_eid}]
    set dest      dtn://host-0/test
    
    dtn::tell_dtnd 0 tcl_registration $dest
    
    puts "* Sending bundle"
    set timestamp [dtn::tell_dtnd $last_node sendbundle $source $dest length=$length]
    
    puts "* Waiting for bundle arrival"
    dtn::wait_for_bundle 0 "$source,$timestamp" 30000

    puts "* Checking bundle data"
    dtn::check_bundle_data 0 "$source,$timestamp" \
	    is_admin 0 source $source dest $dest
    
    puts "* Doing sanity check on stats"
    for {set i 0} {$i <= $last_node} {incr i} {
	dtn::wait_for_bundle_stats $i {0 pending}
	dtn::wait_for_bundle_stats $i {0 expired}
	dtn::wait_for_bundle_stats $i {1 received}
    }
    
    dtn::wait_for_bundle_stats 0 {1 delivered}
    
    for {set i 1} {$i <= $last_node} {incr i} {
	set outgoing_link $clayer-link:$i-[expr $i - 1]
	dtn::wait_for_bundle_stats $i {1 transmitted}
	dtn::wait_for_link_stat $i $outgoing_link 1 bundles_transmitted
	dtn::wait_for_link_stat $i $outgoing_link $length bytes_transmitted
	dtn::wait_for_link_stat $i $outgoing_link 0 bundles_inflight
	dtn::wait_for_link_stat $i $outgoing_link 0 bytes_inflight
    }
	
    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
