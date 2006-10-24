#
#    Copyright 2005-2006 Intel Corporation
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

test::name dtn-ping
net::num_nodes 3

manifest::file apps/dtnping/dtnping dtnping

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ALWAYSON tcp true

global num_pings
set num_pings 10
foreach {var val} $opt(opts) {
    if {$var == "-n" } {
	set num_pings $val
	
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

test::script {
    global num_pings
    
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    set N [net::num_nodes]
    set last_node [expr $N - 1]
    set dest      dtn://host-0/

    for {set i $last_node} {$i >= 0} {incr i -1} {
	puts "* Dtnping'ing from node $last_node to dtn://host-$i\
	    for $num_pings pings (one per second)"
	set pid [dtn::run_app $last_node dtnping "-c $num_pings dtn://host-$i/ping"]
	after [expr ($num_pings -1) * 1000]
	run::wait_for_pid_exit $last_node $pid 30000
    }
    
    for {set i 0} {$i < $last_node} {incr i} {
	puts "* Checking bundle stats on node $i"
	dtn::wait_for_bundle_stats $i [list $num_pings "delivered" \
		$num_pings "generated" \
		[expr $num_pings + ($num_pings * 2 * $i)] "received"] 5000
    }
    
    # Last node is the ping source so it *also* has N * num_pings
    # locally delivered due to the delivery of the ping responses:
    puts "* Checking bundle stats on node $last_node"
    dtn::wait_for_bundle_stats $last_node [list \
	    [expr $num_pings * (1 + $N)]  "delivered" \
	    $num_pings "generated" \
	    [expr $num_pings + ($num_pings * 2 * $last_node) ] "received"] 5000
    
    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
