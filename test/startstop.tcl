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

test::name startstop
net::default_num_nodes 1

dtn::config
dtn::config_interface tcp
dtn::config_linear_topology ONDEMAND tcp true

set clean 0
foreach var $opt(opts) {
    if {$var == "-clean"} {
	set clean 1
    } else {
	puts "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

test::script {
    puts "* Startstop test script executing..."
    dtn::run_dtnd *
    
    puts "* Waiting for dtnd"
    dtn::wait_for_dtnd *
    dtn::tell_dtnd * {puts "startstop test"}

    puts "* Shutting down dtnd"
    dtn::stop_dtnd *

    puts "* Waiting for dtnd to quit"
    run::wait_for_programs

    if {! $clean} {
	puts "* Running new dtnds"
	dtn::run_dtnd *
	
	puts "* Leaving dtn daemons running for the test utils to kill them..."
    }
}
