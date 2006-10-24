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

test::name send-for-two-minutes
net::num_nodes 3

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

test::script {
    
    puts "* running dtnds"
    dtn::run_dtnd *

    puts "* waiting for dtnds to start up"
    dtn::wait_for_dtnd *
    
    set last_node [expr [net::num_nodes] - 1]
    set source    [dtn::tell_dtnd $last_node route local_eid]
    set dest      dtn://host-0/test
    
    dtn::tell_dtnd 0 tcl_registration $dest

    puts "* sending one bundle every 10 seconds for 2 minutes"
    set timestamps {}
    for {set i 1} {$i <= 13} {incr i} {
        puts "* sending bundle $i"
        lappend timestamps [dtn::tell_dtnd $last_node sendbundle $source $dest]
        # save 10 seconds at the end of the test
        if {$i < 13} {
            after 10000
        }
    }
    
    set i 1
    foreach timestamp $timestamps {
        puts -nonewline "* Checking that bundle $i arrived... "
        dtn::wait_for_bundle 0 "$source,$timestamp" 5000
        # the Marv Albert of test reports:
        puts "yes! And it counts!"
        incr i
    }

    puts "* test success!"
}

test::exit_script {
    puts "* stopping all dtnds"
    dtn::stop_dtnd *
}
