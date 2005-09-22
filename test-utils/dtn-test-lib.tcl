
import "dtn-config.tcl"
import "dtn-topology.tcl"

# standard dtn manifest
manifest::file daemon/dtnd dtnd
manifest::file test_utils/dtnd-test-utils.tcl dtnd-test-utils.tcl
manifest::dir  bundles
manifest::dir  db

# options procedure for running dtnd
proc dtnd_opts {id} {
    global opt net::host net::portbase net::extra test::testname

    set dtnd(exec_file) dtnd
    set dtnd(exec_opts) "-i $id -t -c $test::testname.conf"
    set dtnd(confname)  $test::testname.conf
    set dtnd(conf)      [conf::get dtnd $id]

    if {! $opt(xterm)} {
	append dtnd(exec_opts) " -d"
    }

    # XXX/demmer add config hook for console (not just cmdline arg)
    append dtnd(exec_opts) " --console-port [dtn_port console $id]"

    return [array get dtnd]
}

