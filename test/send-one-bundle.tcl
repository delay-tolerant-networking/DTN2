
# test parameter settings
set num_nodes 2
set network loopback
set topology linear

route set type "static"

source "test/main.tcl"

test set initscript {
    log "/test" INFO "route dump:\n[route dump]"
    
    if {$id == 0} {
	
    }
    if {$id == 1} {
	sendbundle
    }
}

