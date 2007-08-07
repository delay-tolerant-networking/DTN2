#
#    Copyright 2007 Intel Corporation
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

test::name dtn-tunnel
net::num_nodes 2

manifest::file apps/dtntest/dtntest dtntest
manifest::file apps/dtntunnel/dtntunnel dtntunnel

dtn::config

dtn::config_interface tcp
dtn::config_linear_topology ALWAYSON tcp true

test::script {
    puts "* Running dtnds"
    dtn::run_dtnd *

    puts "* Running dtntest on node 1"
    dtn::run_dtntest 1

    puts "* Waiting for dtnds to start up"
    dtn::wait_for_dtnd *

    puts "* Setting up flamebox-ignores"
    dtn::tell_dtnd 0 log /test always \
	    "flamebox-ignore ign1 client disconnected without calling dtn_close"
    dtn::tell_dtnd 1 log /test always \
	    "flamebox-ignore ign2 client disconnected without calling dtn_close"

    set client_addr $net::listen_addr(0)
    set client_port [dtn::get_port misc 0]
    set server_addr $net::listen_addr(1)
    set server_port [dtn::get_port misc 1]

    if {$client_addr == "0.0.0.0"} { set client_addr 127.0.0.1 }
    if {$server_addr == "0.0.0.0"} { set server_addr 127.0.0.1 }
    
    puts "* Setting up server callbacks"
    dtn::tell_dtntest 1 proc pingpong {chan} { \
            set ping [gets $chan]; \
            if {[eof $chan]} { puts "$chan closed" ; close $chan ;} \
            else { puts $chan "pong"; } }
    
    dtn::tell_dtntest 1 proc init_pingpong {chan addr port} { \
            global initial_hello; \
            fileevent $chan readable "pingpong $chan"; \
            fconfigure $chan -buffering none; \
            if {[info exists initial_hello]} { puts $chan "hello" } ; }
    
    puts "* Setting up a server socket"
    dtn::tell_dtntest 1 socket -server init_pingpong -myaddr $server_addr $server_port
    
    puts "* Running tunnel listener on node 1"
    set server_pid [dtn::run_app 1 dtntunnel "-L"]
    
    puts "* Running tunnel proxy on node 0"
    set client_pid [dtn::run_app 0 dtntunnel \
            "-T $client_addr:$client_port:$server_addr:$server_port \
            dtn://host-1/dtntunnel"]
    
    puts "* Opening a connection to the tunnel"
    set sock [socket $client_addr $client_port]
    fconfigure $sock -buffering none
    fconfigure $sock -blocking 0

    puts "* Checking bundle stats for connection establishment"
    dtn::wait_for_bundle_stats 1 {1 delivered}
    dtn::wait_for_bundle_stats 0 {1 delivered}

    puts "* Doing a ping/pong"
    puts $sock ping
    dtn::wait_for_bundle_stats 1 {2 delivered}
    dtn::wait_for_bundle_stats 0 {2 delivered}
    set x [gets $sock]
    if {$x != "pong"} {
        puts "ERROR: unexpected reply of $x (not pong)"
    }

    puts "* Doing a ping/pong with an interrupted link"
    tell_dtnd 0 link close tcp-link:0-1
    puts $sock ping
    dtn::wait_for_bundle_stats 0 {1 pending}
    dtn::wait_for_bundle_stats 1 {2 delivered}

    after 2000
    tell_dtnd 0 link open tcp-link:0-1
    dtn::wait_for_bundle_stats 0 {0 pending}
    dtn::wait_for_bundle_stats 1 {3 delivered}
    dtn::wait_for_bundle_stats 0 {3 delivered}

    set x [gets $sock]
    if {$x != "pong"} {
        puts "ERROR: unexpected reply of $x (not pong)"
    }

    puts "* Closing the socket and the client channel"
    close $sock
    after 1000
    run::kill_pid 0 $client_pid TERM
    run::wait_for_pid_exit 0 $client_pid 
    after 1000
    
    tell_dtnd 0 bundle reset_stats
    tell_dtnd 1 bundle reset_stats
    
    puts "* Restarting the client tunnel"
    set client_pid [dtn::run_app 0 dtntunnel \
            "-T $client_addr:$client_port:$server_addr:$server_port \
            dtn://host-1/dtntunnel"]

    puts "* Reopening the socket"
    set sock [socket $client_addr $client_port]
    fconfigure $sock -buffering none
    fconfigure $sock -blocking 0

    puts "* Checking bundle stats for connection establishment"
    dtn::wait_for_bundle_stats 1 {1 delivered}
    dtn::wait_for_bundle_stats 0 {1 delivered}

    puts "* Doing a ping/pong"
    puts $sock ping
    dtn::wait_for_bundle_stats 1 {2 delivered}
    dtn::wait_for_bundle_stats 0 {2 delivered}
    set x [gets $sock]
    if {$x != "pong"} {
        puts "ERROR: unexpected reply of $x (not pong)"
    }

    puts "* Killing the proxy tunnel without closing the socket"
    run::kill_pid 0 $client_pid TERM
    run::wait_for_pid_exit 0 $client_pid 
    after 1000
    set client_pid [dtn::run_app 0 dtntunnel \
            "-T $client_addr:$client_port:$server_addr:$server_port \
            dtn://host-1/dtntunnel"]

    tell_dtnd 0 bundle reset_stats
    tell_dtnd 1 bundle reset_stats
    
    puts "* Opening a new socket"
    set sock [socket $client_addr $client_port]
    fconfigure $sock -buffering none
    fconfigure $sock -blocking 0

    puts "* Checking bundle stats for connection establishment"
    dtn::wait_for_bundle_stats 1 {1 delivered}
    dtn::wait_for_bundle_stats 0 {1 delivered}

    puts "* Doing a ping/pong"
    puts $sock ping
    dtn::wait_for_bundle_stats 1 {2 delivered}
    dtn::wait_for_bundle_stats 0 {2 delivered}
    set x [gets $sock]
    if {$x != "pong"} {
        puts "ERROR: unexpected reply of $x (not pong)"
    }

    puts "* Opening a parallel channel with server-initiated traffic"
    dtn::tell_dtnd 0 bundle reset_stats
    dtn::tell_dtnd 1 bundle reset_stats
    
    dtn::tell_dtntest 1 set initial_hello 1

    set sock2 [socket $client_addr $client_port]
    fconfigure $sock2 -buffering none
    fconfigure $sock2 -blocking 0

    puts "* Checking bundle stats for connection establishment and initial hello"
    dtn::wait_for_bundle_stats 1 {1 delivered}
    dtn::wait_for_bundle_stats 0 {2 delivered}

    puts "* Getting initial hello"
    set x [gets $sock2]
    if {$x != "hello"} {
        puts "ERROR: unexpected reply of $x (not hello)"
    }

    puts "* Doing a ping/pong on second channel"
    puts $sock2 ping
    dtn::wait_for_bundle_stats 1 {2 delivered}
    dtn::wait_for_bundle_stats 0 {3 delivered}

    set x [gets $sock2]
    if {$x != "pong"} {
        puts "ERROR: unexpected reply of $x (not pong)"
    }
    
    puts "* Test success!"
}

test::exit_script {
    puts "* Stopping apps"
    run::kill_pid 1 $server_pid TERM
    run::kill_pid 0 $client_pid TERM
    dtn::stop_dtntest 1

    dtn::tell_dtnd 0 log /test always "flamebox-ignore-cancel ign1"
    dtn::tell_dtnd 1 log /test always "flamebox-ignore-cancel ign2"
    
    puts "* Stopping all dtnds"
    dtn::stop_dtnd *
}
