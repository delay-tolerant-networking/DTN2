#
#    Copyright 2008 Intel Corporation
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

test::name session-membership
net::num_nodes 5

manifest::file apps/dtntest/dtntest dtntest

set testopt(cl)       tcp
run::parse_test_opts

set cl $testopt(cl)

dtn::config --router_type dtlsr --no_null_link --storage_type filesysdb
dtn::config_interface $cl

dtn::config_topology_common false

#
# Set up initial topology:
#
#        0
#        |
#        1
#       / \
#      2   3
#     /
#    4

dtn::config_link 0 1 ALWAYSON $cl ""
dtn::config_link 1 0 ALWAYSON $cl ""
dtn::config_link 1 2 ALWAYSON $cl ""
dtn::config_link 2 1 ALWAYSON $cl ""
dtn::config_link 1 3 ALWAYSON $cl ""
dtn::config_link 3 1 ALWAYSON $cl ""
dtn::config_link 2 4 ALWAYSON $cl ""
dtn::config_link 4 2 ALWAYSON $cl ""
dtn::config_link 3 4 OPPORTUNISTIC $cl ""
dtn::config_link 4 3 OPPORTUNISTIC $cl ""

conf::add dtnd * "route set subscription_timeout 30"

test::script {
    testlog "running dtnd and dtntest"
    dtn::run_dtnd *
    dtn::run_dtntest *

    testlog "waiting for dtnds and dtntest to start up"
    dtn::wait_for_dtnd *
    dtn::wait_for_dtntest *

    set h0 [tell_dtntest 0 dtn_open]
    set h1 [tell_dtntest 1 dtn_open]
    set h2 [tell_dtntest 2 dtn_open]
    set h3 [tell_dtntest 3 dtn_open]
    set h4 [tell_dtntest 4 dtn_open]

    set eid0    dtn://host-0/test
    set eid1    dtn://host-1/test
    set eid2    dtn://host-2/test
    set eid3    dtn://host-3/test
    set eid4    dtn://host-4/test

    set seqid01 <($eid0\ 1)>
    set seqid02 <($eid0\ 2)>
    set seqid03 <($eid0\ 3)>

    set seqid11 <($eid1\ 1)>
    set seqid12 <($eid1\ 2)>
   
    set seqid21 <($eid2\ 1)>
    set seqid22 <($eid2\ 2)>
   
    set g0      dtn-group://group0
    set g1      dtn-group://group1

    testlog "waiting for links to establish"
    dtn::wait_for_link_state 0 $cl-link:0-1 OPEN
    dtn::wait_for_link_state 1 $cl-link:1-0 OPEN
    dtn::wait_for_link_state 1 $cl-link:1-2 OPEN
    dtn::wait_for_link_state 2 $cl-link:2-1 OPEN
    dtn::wait_for_link_state 1 $cl-link:1-3 OPEN
    dtn::wait_for_link_state 3 $cl-link:3-1 OPEN
    dtn::wait_for_link_state 2 $cl-link:2-4 OPEN
    dtn::wait_for_link_state 4 $cl-link:4-2 OPEN
    
    testlog "waiting for routes to settle"
    dtn::wait_for_stats_on_all_links * {0 bundles_inflight 0 bundles_queued}

    dtn::wait_for_route 0 dtn://host-1/* $cl-link:0-1 {}
    dtn::wait_for_route 0 dtn://host-2/* $cl-link:0-1 {}
    dtn::wait_for_route 0 dtn://host-3/* $cl-link:0-1 {}
    dtn::wait_for_route 0 dtn://host-4/* $cl-link:0-1 {}

    dtn::wait_for_route 1 dtn://host-0/* $cl-link:1-0 {}
    dtn::wait_for_route 1 dtn://host-2/* $cl-link:1-2 {}
    dtn::wait_for_route 1 dtn://host-3/* $cl-link:1-3 {}
    dtn::wait_for_route 1 dtn://host-4/* $cl-link:1-2 {}
    
    dtn::wait_for_route 2 dtn://host-0/* $cl-link:2-1 {}
    dtn::wait_for_route 2 dtn://host-1/* $cl-link:2-1 {}
    dtn::wait_for_route 2 dtn://host-3/* $cl-link:2-1 {}
    dtn::wait_for_route 2 dtn://host-4/* $cl-link:2-4 {}
    
    dtn::wait_for_route 3 dtn://host-0/* $cl-link:3-1 {}
    dtn::wait_for_route 3 dtn://host-1/* $cl-link:3-1 {}
    dtn::wait_for_route 3 dtn://host-2/* $cl-link:3-1 {}
    dtn::wait_for_route 3 dtn://host-4/* $cl-link:3-1 {}

    dtn::wait_for_route 4 dtn://host-0/* $cl-link:4-2 {}
    dtn::wait_for_route 4 dtn://host-1/* $cl-link:4-2 {}
    dtn::wait_for_route 4 dtn://host-2/* $cl-link:4-2 {}
    dtn::wait_for_route 4 dtn://host-3/* $cl-link:4-2 {}

    testlog "sanity checking stats"
    dtn::check_bundle_stats 0 {5 pending 0 expired 0 duplicate}
    dtn::check_bundle_stats 1 {5 pending 0 expired 0 duplicate}
    dtn::check_bundle_stats 2 {5 pending 0 expired 0 duplicate}
    dtn::check_bundle_stats 3 {5 pending 0 expired 0 duplicate}
    dtn::check_bundle_stats 4 {5 pending 0 expired 0 duplicate}

    testlog "creating custodian registration on node 1"
    set custodian_regid [tell_dtntest 1 dtn_register $h1 \
            endpoint=$g0 expiration=0 session_flags=custody]
    
    testlog "creating a subscriber registration on node 3"
    set subscriber_3 [tell_dtntest 3 dtn_register $h3 \
            endpoint=$g0 expiration=0 session_flags=subscribe]

    testlog "checking that custodian is notified"
    set L [tell_dtntest 1 dtn_session_update $h0 10000]
    testlog "result $L"

    testlog "creating a publisher registration on node 0"
    set publisher_0 [tell_dtntest 0 dtn_register $h0 \
            endpoint=$g0 expiration=0 session_flags=publish]

    testlog "sending a bundle, checking that it is delivered"
    tell_dtntest 0 dtn_send $h0 regid=$publisher_0 \
            source=$g0 dest=$g0 expiration=3600 payload_data="bundle1" \
            sequence_id=$seqid01

    array set bundle_data [dtn::tell_dtntest 3 dtn_recv $h3 \
            payload_mem=true timeout=10000]
    dtn::check string equal $bundle_data(sequence_id) $seqid01

    testlog "checking that it is pending at subscribed nodes"
    dtn::wait_for_bundle_stats 0 {6 pending}
    dtn::wait_for_bundle_stats 1 {6 pending}
    dtn::wait_for_bundle_stats 3 {6 pending}

    testlog "sending an obsoleting bundle, checking that it is delivered"
    tell_dtntest 0 dtn_send $h0 regid=$publisher_0 \
            source=$g0 dest=$g0 expiration=3600 payload_data="bundle2" \
            sequence_id=$seqid02 obsoletes_id=$seqid01

    array set bundle_data [dtn::tell_dtntest 3 dtn_recv $h3 \
            payload_mem=true timeout=10000]
    dtn::check string equal $bundle_data(sequence_id) $seqid02

    testlog "testing that it obsoleted the old one"
    dtn::wait_for_bundle_stats 1 {6 pending}
    dtn::wait_for_bundle_stats 3 {6 pending}

    testlog "sanity checking stats"
    dtn::check_bundle_stats 0 {6 pending 0 expired 0 duplicate}
    dtn::check_bundle_stats 1 {6 pending 0 expired 0 duplicate}
    dtn::check_bundle_stats 2 {5 pending 0 expired 0 duplicate}
    dtn::check_bundle_stats 3 {6 pending 0 expired 0 duplicate}
    dtn::check_bundle_stats 4 {5 pending 0 expired 0 duplicate}

    testlog "subscribing at node 4"
    set subscriber_4 [tell_dtntest 4 dtn_register $h4 \
            endpoint=$g0 expiration=0 session_flags=subscribe]

    testlog "checking that only the second is delivered"
    array set bundle_data [dtn::tell_dtntest 4 dtn_recv $h4 \
            payload_mem=true timeout=10000]
    dtn::check string equal $bundle_data(sequence_id) $seqid02
    if {! [catch {
        dtn::tell_dtntest 4 dtn_recv $h4 payload_mem=true timeout=1000
    } err]} {
        error "dtn_recv should have timed out"
    }

    testlog "checking it is cached at the newly subscribed nodes"
    dtn::wait_for_bundle_stats 1 {6 pending}
    dtn::wait_for_bundle_stats 2 {6 pending}
    dtn::wait_for_bundle_stats 3 {6 pending}
    dtn::wait_for_bundle_stats 4 {6 pending}
    
    testlog "sanity checking stats"
    dtn::check_bundle_stats 0 {6 pending 0 expired 0 duplicate}
    dtn::check_bundle_stats 1 {6 pending 0 expired 0 duplicate}
    dtn::check_bundle_stats 2 {6 pending 0 expired 0 duplicate}
    dtn::check_bundle_stats 3 {6 pending 0 expired 0 duplicate}
    dtn::check_bundle_stats 4 {6 pending 0 expired 0 duplicate}

    testlog "sending a non-obsoleting bundle"
    tell_dtntest 0 dtn_send $h0 regid=$publisher_0 \
            source=$g0 dest=$g0 expiration=3600 payload_data="bundle2" \
            sequence_id=$seqid11

    testlog "waiting for it to be delivered"
    array set bundle_data [dtn::tell_dtntest 3 dtn_recv $h3 \
            payload_mem=true timeout=10000]
    dtn::check string equal $bundle_data(sequence_id) $seqid11
    array set bundle_data [dtn::tell_dtntest 4 dtn_recv $h4 \
            payload_mem=true timeout=10000]
    dtn::check string equal $bundle_data(sequence_id) $seqid11

    testlog "changing topology so that node 4 is now attached to node 3"
    tell_dtnd 2 link close $cl-link:2-4
    tell_dtnd 3 link open $cl-link:3-4

    testlog "waiting for routes to settle"
    dtn::wait_for_stats_on_all_links * {0 bundles_inflight 0 bundles_queued}
    
    dtn::wait_for_route 0 dtn://host-1/* $cl-link:0-1 {}
    dtn::wait_for_route 0 dtn://host-2/* $cl-link:0-1 {}
    dtn::wait_for_route 0 dtn://host-3/* $cl-link:0-1 {}
    dtn::wait_for_route 0 dtn://host-4/* $cl-link:0-1 {}

    dtn::wait_for_route 1 dtn://host-0/* $cl-link:1-0 {}
    dtn::wait_for_route 1 dtn://host-2/* $cl-link:1-2 {}
    dtn::wait_for_route 1 dtn://host-3/* $cl-link:1-3 {}
    dtn::wait_for_route 1 dtn://host-4/* $cl-link:1-3 {}
    
    dtn::wait_for_route 2 dtn://host-0/* $cl-link:2-1 {}
    dtn::wait_for_route 2 dtn://host-1/* $cl-link:2-1 {}
    dtn::wait_for_route 2 dtn://host-3/* $cl-link:2-1 {}
    dtn::wait_for_route 2 dtn://host-4/* $cl-link:2-1 {}
    
    dtn::wait_for_route 3 dtn://host-0/* $cl-link:3-1 {}
    dtn::wait_for_route 3 dtn://host-1/* $cl-link:3-1 {}
    dtn::wait_for_route 3 dtn://host-2/* $cl-link:3-1 {}
    dtn::wait_for_route 3 dtn://host-4/* $cl-link:3-4 {}

    dtn::wait_for_route 4 dtn://host-0/* $cl-link:4-3 {}
    dtn::wait_for_route 4 dtn://host-1/* $cl-link:4-3 {}
    dtn::wait_for_route 4 dtn://host-2/* $cl-link:4-3 {}
    dtn::wait_for_route 4 dtn://host-3/* $cl-link:4-3 {}
    
}

test::exit_script {
    testlog "stopping all dtnds and dtntest"
    dtn::stop_dtnd *
    dtn::stop_dtntest *
}
