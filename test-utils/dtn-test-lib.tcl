
proc dtnd_opts {id} {
    global net::host net::portbase net::extra test::testname
    
    set dtnd(exec_file) dtnd
    set dtnd(exec_opts) "-i $id -t -c $test::testname.conf"
    set dtnd(confname)  $test::testname.conf
    set dtnd(conf)      [conf::get dtnd $id]

    return [array get dtnd]
}

