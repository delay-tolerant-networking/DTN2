
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

set blocksz 8192

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

proc ack_arrived {sock} {
    global got_ack ack_timer
    after cancel $ack_timer
    set got_ack 1

    set ack [gets $sock]
    puts "ack_arrived: $ack"
}

proc ack_timeout {} {
    global got_ack
    set got_ack 0
    puts "ack_timeout"
}

proc send_file {host file} {
    global port
    global logfd
    global blocksz
    global got_ack
    global ack_timer
    set fd [open $file]

    puts "sending file $file size [file size $file]"
    puts $logfd "[time] :: sending file [file tail $file]   " 
    flush $logfd
    while  { [  catch {socket $host $port } sock] } {
	puts "Trying to connect, will try again after  2 seconds "
	after 2000
    }
    
    fconfigure $sock -translation binary
    fconfigure $sock -encoding binary
    
    if [catch {
	puts $sock "[file tail $file] [file size $file]"
    }] {
	return -1
    }

    while {![eof $fd]} {
	if {[catch {
	    set payload [read $fd $blocksz]
#	    puts "sending [string length $payload] byte chunk"
	    puts -nonewline $sock $payload
	    flush $sock
	} ]} {
	    return -1
	}
    }	

    close $fd
    
    # wait for an ack or timeout
    puts "sent whole file, waiting for ack"
    fconfigure $sock -blocking 0
    set got_ack -1
    fileevent $sock readable "ack_arrived $sock"
    set ack_timer [after 5000 ack_timeout]
    vwait got_ack

    close $sock

    if {$got_ack} {
	puts "[time] :: file  actually sent $file"
	return 1
    } else {
	puts "[time] :: file sent but not acked"
	return -1
    }
}

proc recv_files {dest_dir} {
    global port
    global conns
    puts "waiting for files:  $dest_dir"
    set conns(main) [socket -server "new_client $dest_dir" $port]
}


proc new_client {dest_dir sock addr port} {
    global conns
    global length_remaining

    puts " Accept $sock from $addr port $port"
    
    # Used for debugging
    set conns(addr,$sock) [list $addr $port]

    set L [gets $sock]
    foreach {filename length} $L {}



    puts "getting file $filename length $length"
    
    set to_fd [open "$dest_dir/$filename" w]
    set length_remaining($sock) $length
    
    fileevent $sock readable [list file_arrived $filename $to_fd $dest_dir $sock]
    fconfigure $sock -blocking 0 
    fconfigure $sock -translation binary
    fconfigure $sock -encoding binary
}


proc file_arrived {file to_fd dest_dir sock} {
    
    global logfd
    global conns
    global length_remaining
    
    if {[eof $sock]} {
	puts "[time] Normal Close $conns(addr,$sock) EOF received"
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
	set got [string length $payload]
	set todo [expr $length_remaining($sock) - $got]
#	puts "got $got byte chunk ($todo to go)"

	if {$todo < 0} {
	    error "negative todo"
	}
	
	if {$todo == 0} {
	    puts $sock "ack file $file"
	    flush $sock
	    puts "[time] sending ack for file $file"
	}
	
	set length_remaining($sock) $todo
    }
}


# Define a bgerror proc to print the error stack when errors occur in
# event handlers
proc bgerror {err} {
    global errorInfo
    puts "tcl error: $err\n$errorInfo"
}

proc usage {} {
    puts "usage: simple_ftp server <dir> <logfile>"
    puts "usage: simple_ftp client <dir> <logfile> <host>"
    exit -1
}

set argc [llength $argv]
if {$argc < 3} {
    usage
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
