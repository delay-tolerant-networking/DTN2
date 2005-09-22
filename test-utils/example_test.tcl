net::set_nodes 2
test::set_name example

manifest::file "test_utils/example_test.tcl" "f/o/o/foo"
manifest::dir  "bundles"
manifest::dir  "db"
manifest::dir  "bar"

conf::add 0 {
    puts "Put node 0 configuration here"

    storage set type berkeleydb
    storage set payloaddir bundles
    storage set dbname     DTN
    storage set dbdir      db
    storage set dberrlog   DTN.errlog

    route set type static
    route local_eid "dtn://[info hostname].dtn"
}

conf::add 1 {
    puts "Put node 1 configuration here"

    storage set type berkeleydb
    storage set payloaddir bundles
    storage set dbname     DTN
    storage set dbdir      db
    storage set dberrlog   DTN.errlog
    
    route set type static
    route local_eid "dtn://[info hostname].dtn"
}

for {set i 0} {$i < 2} {incr i} {
    conf::add $i "api set local_port $net::portbase($i)\n"
}

test::decl {
    puts "Put your test actions here"
}