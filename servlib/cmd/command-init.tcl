#
# This file is converted into a big C string during the build
# process and evaluated in the command interpreter at startup
# time.
#

# For the vwait to work, we need to make sure there's at least one
# event outstanding at all times, otherwise the 'vwait forever' trick
# below doesn't work
proc event_sched {} {
    after 1000000 event_sched
}

# Run the event loop and no interpreter
proc event_loop {} {
    event_sched
    global forever
    vwait forever
}
    
# Run the command loop with the given prompt
proc command_loop {prompt} {
    if [catch {
	package require tclreadline
	global command_prompt
	set command_prompt $prompt
	namespace eval tclreadline {
	    proc prompt1 {} {
		global command_prompt
		return "${command_prompt}% "
	    }
	}

	tclreadline::Loop
    } err] {
	log /tcl WARNING "can't load tclreadline: $err"
	log /tcl WARNING "no command loop available"

	event_loop
    }
}

# Define a bgerror proc to print the error stack when errors occur in
# event handlers
proc bgerror {err} {
    global errorInfo
    puts "tcl error: $err\n$errorInfo"
}

