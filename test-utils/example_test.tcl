net::num_nodes 2
test::name example

# To add additional files, include the following:
# manifest::file "test_utils/example_test.tcl" "f/o/o/foo"

config_berkeleydb
config_interface tcp
config_linear_topology ONDEMAND

conf::add dtnd 0 {
    puts ""
    puts "dtnd id 0 starting up..."
    puts ""

    storage set type berkeleydb
    storage set payloaddir bundles
    storage set dbname     DTN
    storage set dbdir      db
    storage set dberrlog   DTN.errlog

    route set type static
    route local_eid "dtn://[info hostname].dtn"
}

conf::add dtnd 1 {
    puts ""
    puts "dtnd id 1 starting up..."
    puts ""

    
    route set type static
    route local_eid "dtn://[info hostname].dtn"
}

for {set i 0} {$i < 2} {incr i} {
    conf::add dtnd $i "api set local_port $net::portbase($i)\n"
}

test::script {
    puts ""
    puts "Example test script"
    puts ""

    
}
