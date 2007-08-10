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

test::name dtlsr-areas
net::num_nodes 5

dtn::config --router_type dtlsr --no_null_link

set cl tcp

foreach {var val} $opt(opts) {
    if {$var == "-cl" || $var == "cl"} {
	set cl $val
    } else {
	testlog "ERROR: unrecognized test option '$var'"
	exit 1
    }
}

testlog "Configuring $cl interfaces / links"
dtn::config_interface $cl
dtn::config_linear_topology ALWAYSON $cl false

conf::add dtnd 0 { route set dtlsr_area aaa }
conf::add dtnd 1 { route set dtlsr_area aaa }
conf::add dtnd 2 { route set dtlsr_area bbb }
conf::add dtnd 3 { route set dtlsr_area bbb }
conf::add dtnd 4 { route set dtlsr_area ccc }

test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    testlog "Adding registrations"
    foreach node [net::nodelist] {
        dtn::tell_dtnd $node tcl_registration dtn://$node/test
    }

    testlog "Waiting for routes to settle"
    after 5000
    
    testlog "Checking that routes only exist within each area"
    dtn::check_route    0 dtn://host-1/* $cl-link:0-1 {}
    dtn::check_route    0 dtn://host-2/* $cl-link:0-1 {}
    dtn::check_no_route 0 dtn://host-3/*
    dtn::check_no_route 0 dtn://host-4/*

    dtn::check_route    1 dtn://host-0/* $cl-link:1-0 {}
    dtn::check_route    1 dtn://host-2/* $cl-link:1-2 {}
    dtn::check_no_route 1 dtn://host-3/*
    dtn::check_no_route 1 dtn://host-4/*

    dtn::check_no_route 2 dtn://host-0/*
    dtn::check_route    2 dtn://host-1/* $cl-link:2-1 {}
    dtn::check_route    2 dtn://host-3/* $cl-link:2-3 {}
    dtn::check_route    2 dtn://host-4/* $cl-link:2-3 {}

    dtn::check_no_route 3 dtn://host-0/*
    dtn::check_route    3 dtn://host-1/* $cl-link:3-2 {}
    dtn::check_route    3 dtn://host-2/* $cl-link:3-2 {}
    dtn::check_route    3 dtn://host-4/* $cl-link:3-4 {}

    dtn::check_no_route 4 dtn://host-0/*
    dtn::check_no_route 4 dtn://host-1/*
    dtn::check_no_route 4 dtn://host-2/*
    dtn::check_route    4 dtn://host-3/* $cl-link:4-3 {}

    
    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
