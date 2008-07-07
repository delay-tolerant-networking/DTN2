#!/usr/bin/tclsh

#
#    Copyright 2005-2008 Intel Corporation
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

#
# Simple client/server file transfer protocol
#

proc usage {} {
    puts "usage: simple_ftp server [opts]"
    puts "usage: simple_ftp client <file> <copies> <host> [opts]"
    exit 1
}

set port 17600
set blocksz 1024
set delay 1000
set data_timeout 10000

set debug 0
proc log {msg} {
    global debug
    if {$debug} {
        puts "[clock seconds]: $msg"
    }
}

proc start_file {} {
    global file sent copies
    if {$sent == $copies} {
        log "start_file: $sent copies already sent..."
        return
    }

    global fd
    if [info exists fd] {
        catch {close $fd}
    }
    if [catch {
        set fd [open $file r]
    } err] {
        puts "error opening file $file: $err"
        exit 1
    }

    global sock host port delay
    if {! [info exists sock]} {
	if { [catch {
            set sock [socket $host $port]
        } err] } {
	    puts "[clock seconds]: failed to connect... will try again after $delay ms"
	    after $delay start_file
            return
	}
        
	log "connected..."
	fconfigure $sock -translation binary
	fconfigure $sock -encoding binary
        fconfigure $sock -buffering none
	fconfigure $sock -blocking false
    }

    # send the header
    set length [file size $file]
    puts $sock "[file tail $file] [expr $sent + 1] $length"

    fileevent $sock writable send_data
    fileevent $sock readable handle_data

    global data_timeout no_data_timer
    if {$data_timeout != 0 && ![info exists no_data_timer]} {
        set no_data_timer [after $data_timeout handle_data_timeout]
    }
}

proc send_data {} {
    global sock fd blocksz sent delay

    set data [read $fd $blocksz]

    if {[string length $data] != 0} {
        log "sending [string length $data] byte chunk"
        puts -nonewline $sock $data
    }
    
    if [eof $fd] {
        puts "[clock seconds]: file $sent transmitted"
        close $fd
        fileevent $sock writable ""
        incr sent
        after $delay start_file
        return
    }
    
}

proc handle_data {} {
    global sock
    if [catch {
	set data [gets $sock]
    } err] {
	log "data arrived but error reading from socket!"
        close_and_restart
        return
    }
    
    if {[eof $sock]} {
	puts "[clock seconds] EOF on socket"
        close_and_restart
	return
    }
    
    global data_timeout no_data_timer
    if {$data_timeout != 0} {
        after cancel $no_data_timer
        set no_data_timer [after $data_timeout handle_data_timeout]
    }

    if {$data == ""} {
        log "warning: got empty data"
        return
    }

    if {$data == "."} {
        log "KEEPALIVE"
        return
    }
    
    if {[lindex $data 0] != "ack"} {
	log "ERROR in ack_arrived: got '$data', expected ack"
        exit 1
    }
    
    set num [lindex $data 1]
    puts "[clock seconds]: file $num acked"

    global acked copies
    if {$num != [expr $acked + 1]} {
        log "sync error... ack for $num but acked == $acked"
    }

    set acked $num
    
    if {$acked == $copies} {
        puts "[clock seconds]: all $copies copies sent and acked"
        exit 0
    }
}


proc handle_data_timeout {} {
    puts "[clock seconds]: timeout waiting for incoming data"
    close_and_restart
}

proc cleanup_sock {sock} {
    catch {
        fileevent $sock writable ""
        fileevent $sock readable ""
        close $sock
    }
}

proc close_and_restart {} {
    global sock sent acked no_data_timer delay

    if [info exists sock] {
        cleanup_sock $sock
        unset sock
    }
    
    # be careful... at this point we need to change the sent
    # marker to equal the acked marker so that we properly re-send
    # the files
    set sent $acked

    if [info exists no_data_timer] {
        after cancel $no_data_timer
        unset no_data_timer
    }

    after $delay start_file
}

proc start_server {} {
    global port
    puts "[clock seconds]: starting server om port $port..."
    socket -server "new_client" $port
}

proc new_client {sock addr port} {
    puts "[clock seconds]: new client from $addr port $port"
    
    fconfigure $sock -translation binary
    fconfigure $sock -encoding binary
    fconfigure $sock -blocking false
    fconfigure $sock -buffering none
    
    fileevent $sock readable "header_arrived $sock"
}

proc header_arrived {sock} {
    set l [gets $sock]
    if [eof $sock] {
        log "eof on socket $sock"
        fileevent $sock readable ""
        close $sock
        return
    }

    if {$l == ""} {
        log "warning: got empty line..."
        return
    }
        
    if {[llength $l] != 3} {
        log "protocol error: got header line $l"
        exit 1
    }

    log "incoming file $l"
    set file   [lindex $l 0]
    set num    [lindex $l 1]
    set length [lindex $l 2]

    set fd [open "$file.$num" w]
    global todo
    set todo($sock) $length
    fileevent $sock readable "data_arrived $sock $file $fd $num $length"
}

proc data_arrived {sock file fd num length} {
    global todo

    if [catch {
        set data [read $sock $todo($sock)]
    } err] {
        log "error reading from $sock: $err"
        cleanup_sock $sock
        return
    }
    
    if {[eof $sock]} {
	puts "[clock seconds]: eof on $sock"
        cleanup_sock $sock
	return
    } 

    set rcvd [string length $data]
    puts -nonewline $fd $data

    incr todo($sock) -$rcvd
    log "got ${rcvd}/${length} bytes ($todo($sock) todo)"

    if {$todo($sock) < 0} {
	error "negative todo"
    }
    
    if {$todo($sock) == 0} {
	puts "[clock seconds] got complete file '$file' copy $num... sending ack"
	close $fd
	puts $sock "ack $num"
	fileevent $sock readable "header_arrived $sock"
    }
}

# Define a bgerror proc to print the error stack when errors occur in
# event handlers
proc bgerror {err} {
    global errorInfo
    puts "tcl error: $err\n$errorInfo"
}

# ARGS
set argc [llength $argv]
if {$argc < 1} {
    usage
}

proc parse_opts {opts} {
    foreach {var val} $opts {
        if {$var == "-port"} {
            global port
            set port $val

        } elseif {$var == "-timeout"} {
            global data_timeout
            set data_timeout $val

        } else {
            error "invalid option $var"
        }
    }
}

set mode [lindex $argv 0]
if {$mode == "server"} {
    parse_opts [lrange $argv 1 end]
    start_server
} elseif {$mode == "client"} {
    set file   [lindex $argv 1]
    set copies [lindex $argv 2]
    set host   [lindex $argv 3]
    parse_opts [lrange $argv 4 end]

    set sent  0
    set acked 0
    puts "[clock seconds]: client sending $copies copies of $file to $host:$port"
    start_file
} else {
    error "unknown mode $mode"
}

vwait forever
