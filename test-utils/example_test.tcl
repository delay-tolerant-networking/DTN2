net::set_nodes 2
test::set_name example

manifest::file "test_utils/example_test.tcl" "f/o/o/foo"
manifest::dir  "bar"

conf::add 0 {
    puts "Put node 0 configuration here"
}

conf::add 1 {
    puts "Put node 1 configuration here"
}

test::decl {
    puts "Put your test actions here"
}