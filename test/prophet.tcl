#
#    Copyright 2007 Baylor University
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

# name of test
test::name prophet

# XXX/wilson automate these and other args
# number of participating nodes
net::num_nodes 3
set length 5000
set cl tcp

dtn::config --router_type prophet --no_null_link

testlog "Configuring tcp interfaces / links"
dtn::config_interface $cl
dtn::config_linear_topology OPPORTUNISTIC $cl false

set link1 $cl-link:1-0
set link2 $cl-link:2-1

# Test plan:
#   Send bundle from 2 to 0 before any routes are discovered
#   Bring up link from 1 to 0 to introduce the route
#   Bring down link from 1 to 0
#   Bring up link from 2 to 1 to introduce the route
#   Bring down link from 2 to 1
#   Bring up link from 1 to 0
#   Check that bundle is delivered to 0

test::script {
    testlog "Running dtnds"
    dtn::run_dtnd *

    testlog "Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

#   Send bundle from 2 to 0 before any routes are discovered
    set source dtn://host-2
    set dest   dtn://host-0/test
    dtn::tell_dtnd 0 tcl_registration $dest

    testlog "Sending bundle from 2 to 0"
    set timestamp [dtn::tell_dtnd 2 sendbundle $source $dest length=$length]

#   Bring up link from 1 to 0 to introduce the route
    testlog "Tickle $link1 on 1 to emulate discovery"
    dtn::tell_dtnd 1 link set_available $link1 true

    # wait for stuff to happen
    after 2000

#   Bring down link from 1 to 0
    testlog "Disable $link1 on 1 to emulate \"out of range\""
    dtn::tell_dtnd 1 link set_available $link1 false

    # now 1 and 0 know each other

#   Bring up link from 2 to 1 to introduce the route
    testlog "Tickle $link2 on 2 to emulate discovery"
    dtn::tell_dtnd 2 link set_available $link2 true

    # wait for stuff to happen
    after 2000

#   Bring down link from 2 to 1
    testlog "Disable $link2 on 2 to emulate \"out of range\""
    dtn::tell_dtnd 2 link set_available $link2 false

#   Bring up link from 1 to 0
    testlog "Tickle $link1 on 1 to emulate discovery"
    dtn::tell_dtnd 1 link set_available $link1 true

    # wait for stuff to happen
    after 2000

#   Check that bundle is delivered to 0
    testlog "Waiting for bundle arrival on 0"
    dtn::wait_for_bundle 0 "$source,$timestamp" 30
    testlog "Checking bundle data"
    dtn::check_bundle_data 0 "$source,$timestamp" \
         is_admin 0 source $source dest $dest

    dtn::wait_for_bundle_stats 0 {1 delivered}

    testlog "Test success!"
}

test::exit_script {
    testlog "Stopping all dtnds"
    dtn::stop_dtnd *
}
