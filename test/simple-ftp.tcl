
#
# Simple client/server code that scans a directory for new data and sends
# that data to a remote host.
#
# usage: simple_ftp server <dir> <logfile>
# usage: simple_ftp client <dir> <logfile> <host>
#
#

set port 17600
set period 1000


proc time {} {
    return [clock seconds]
}

proc timef {} {
    return [clock format [clock seconds]]
}

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

	set sent_file 0
	while {$sent_file != 1} {
	    set sent_file [send_file $host $file]
	}

	file delete -force $file
    }

    after $period scan_dir $host $dir
}

proc send_file {host file} {
    global port
    global logfd
    set fd [open $file]
    set payload [read $fd]
    close $fd

    puts "sending file $file"
    puts $logfd "[time] :: sending file [file tail $file]   " 
    flush $logfd
    while  { [  catch {socket $host $port } sock] } {
	puts "Trying to connect, will try again after  2 seconds "
	after 2000
    }

    if  {[ catch {puts $sock "[file tail $file]" } ]} { return -1 ; }
    flush $sock
    if  {[ catch {puts -nonewline $sock $payload } ]} { return -1 ; }
    flush $sock

    close $sock
    puts " file  actually sent $file"
    return 1;
}

proc recv_files {dest_dir} {
    global port
    global conns
    puts "waiting for files:  $dest_dir"
    set conns(main) [socket -server "new_client $dest_dir" $port]
}


proc new_client {dest_dir sock addr port} {
    
    global conns

    puts " Accept $sock from $addr port $port"
    
    # Used for debugging
    set conns(addr,$sock) [list $addr $port]
    set filename [gets $sock]
    set to_fd [open "$dest_dir/$filename" w]

    fileevent $sock readable [list file_arrived $filename $to_fd $dest_dir $sock]
    fconfigure $sock -buffering line
    fconfigure $sock -blocking 0 
    
}


proc file_arrived {file to_fd dest_dir sock} {
    
    global logfd
    global conns
    if {[eof $sock]} {
	puts "Normal Close $conns(addr,$sock) EOF received"
	close $sock
	close $to_fd
	puts $logfd "[time] :: got file [file tail $file]  at [timef]" 
	flush $logfd
	unset conns(addr,$sock)
	
    } else {
	#[catch {gets $sock line}]
	#	    puts "Abnormal close $conns(addr,$sock)"
	set payload [read $sock]
	puts -nonewline $to_fd $payload
    }
}


set mode [lindex $argv 0]
set dir  [lindex $argv 1]
set logfile  [lindex $argv 2]
set logfd [open $logfile w]

puts "Starting in $mode, dir is $dir at [timef]" 


if {$mode == "server"} {
    recv_files $dir
} elseif {$mode == "client"} {
    set host [lindex $argv 3]
    scan_dir $host $dir
} else {
    error "unknown mode $mode"
}




vwait forever
