
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

	send_file $host $file

	file delete $file
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
	puts "Trying to connect, will try again after  5 seconds "
	after 5000
    }
    
puts $sock "[file tail $file]"
    puts -nonewline $sock $payload
    flush $sock
    close $sock
}

proc recv_files {dest_dir} {
    global port
    puts "waiting for files:  $dest_dir"
    socket -server "file_arrived $dest_dir" $port
}

proc file_arrived {dest_dir sock addr port} {
    global logfd
    set file [gets $sock]
    set payload [read $sock]
    close $sock

    puts "got file $file"
    puts $logfd "[time] :: got file [file tail $file]  at [timef]" 
    flush $logfd
    set fd [open "$dest_dir/$file" w]
    puts -nonewline $fd $payload
    close $fd
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
