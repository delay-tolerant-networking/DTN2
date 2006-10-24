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

    puts "* Checking link is OPENING or UNAVAILABLE"
    dtn::wait_for_link_state 0 bogus {OPENING UNAVAILABLE}

    puts "* Closing bogus link"
    dtn::tell_dtnd 0 link close bogus

    puts "* Checking link is UNAVAILABLE"
    dtn::wait_for_link_state 0 bogus UNAVAILABLE

    puts "* Reopening link"
    dtn::tell_dtnd 0 link open bogus
    
    puts "* Checking link is OPENING or UNAVAILABLE"
    dtn::wait_for_link_state 0 bogus {OPENING UNAVAILABLE}
    
    puts "* Closing bogus link again"
    dtn::tell_dtnd 0 link close bogus

    puts "* Checking link is UNAVAILABLE"
    dtn::wait_for_link_state 0 bogus UNAVAILABLE
    
    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
