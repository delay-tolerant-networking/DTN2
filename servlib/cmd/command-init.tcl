#
# This file is converted into a big C string during the build
# process and evaluated in the command interpreter at startup
# time.
#

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
	
	global forever
	vwait forever
    }
}

# Define a bgerror proc to print the error stack when errors occur in
# event handlers
proc bgerror {err} {
    global errorInfo
    puts "tcl error: $err\n$errorInfo"
}

