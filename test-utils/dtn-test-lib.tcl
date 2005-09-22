
import "dtn-config.tcl"
import "dtn-topology.tcl"

# standard dtn manifest
manifest::file daemon/dtnd dtnd
manifest::file test_utils/dtnd-test-utils.tcl dtnd-test-utils.tcl
manifest::dir  bundles
manifest::dir  db

# standard config for all dtnd's is to source the test utilities
conf::add dtnd * "source \"dtnd-test-utils.tcl\""

# options procedure for running dtnd
proc dtnd_opts {id} {
    global net::host net::portbase net::extra test::testname
    
    set dtnd(exec_file) dtnd
    set dtnd(exec_opts) "-i $id -t -c $test::testname.conf"
    set dtnd(confname)  $test::testname.conf
    set dtnd(conf)      [conf::get dtnd $id]

    return [array get dtnd]
}

