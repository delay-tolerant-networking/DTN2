
set port 17600
set period 1000

proc scan_dir {host dir} {
    global period

    puts "scanning dir $dir..."
    set files [glob -nocomplain -- "$dir/*"]
    
    foreach file $files {
	file stat $file file_stat
	
	if {$file_stat(type) != "file"} {
	    continue
	}

	set len $file_stat(size)
	if {$len == 0} {
	    continue
	}

	send_file $host $file

	file delete $file
    }

    after $period scan_dir $host $dir
}

proc send_file {host file} {
    global port
    set fd [open $file]
    set payload [read $fd]
    close $fd

    puts "sending file $file"
    set sock [socket $host $port]
    puts $sock "[file tail $file]"
    puts -nonewline $sock $payload
    flush $sock
    close $sock
}

proc recv_files {dest_dir} {
    global port
    puts "waiting for files: dest $dest_dir"
    socket -server "file_arrived $dest_dir" $port
}

proc file_arrived {dest_dir sock addr port} {
    set file [gets $sock]
    set payload [read $sock]
    close $sock

    puts "got file $file"
    set fd [open "$dest_dir/$file" w]
    puts -nonewline $fd $payload
    close $fd
}

set mode [lindex $argv 0]
set dir  [lindex $argv 1]

if {$mode == "server"} {
    recv_files $dir
} elseif {$mode == "client"} {
    set host [lindex $argv 2]
    scan_dir $host $dir
} else {
    error "unknown mode $mode"
}


vwait forever
