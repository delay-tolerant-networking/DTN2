# Start of Base Script to setup Emulab DTN2 experiments

# Use: cat base-emu.tcl | sed 's/711/finishtime' 

set delay 5ms
set queue DropTail
set protocol Static



### time to finish one run of the experiment
#set finish 711
set linkdynamics 0

## Length of uptime
set up 70
## Length of downtime
set down 40  
set WARMUPTIME 10

set OFFSET_VAL $up



## generates output in two lists up and down 
proc sched {offset} {
global MAX_SIM_TIME
global ONE_CYCLE_LENGTH
global up
global down
global uplist
global downlist
## locals are only start, current, 
set uplist {}
set downlist {}
set start 0 

## Assumes that the link is up.
## The first event schedules is a down link event

while {$start < $MAX_SIM_TIME} {
   # puts "Outer loop $start"
    set current [expr $start + $offset]
    set limit [expr $start + $ONE_CYCLE_LENGTH ]
    while {$current  < $limit} {
	set current [expr $current + $up]
	if {$current > $limit} break ; 
	lappend downlist $current
	set current [expr $current + $down]
	if {$current > $limit} break ; 
	lappend uplist $current
    }
    set start [expr $start + $ONE_CYCLE_LENGTH ]
}
}

## get cmd to be executed on node $id. 
proc get-cmd {id perhopl protol} {


    global maxnodes
    global nfiles
    global size
    global exp

    set valperhopl 1
    if {$perhopl == "ph"}  { set valperhopl  1};
    if {$perhopl == "e2e"} { set valperhopl  0};
    
    set args "$id $exp $maxnodes $nfiles $size $valperhopl $protol"
    
   set prefix /proj/DTN/nsdi/DTN2
    return "$prefix/test/base-cmd.sh $args "
 
}



# This is a simple ns script that demonstrates loops.
set ns [new Simulator]                  
source tb_compat.tcl
set os rhl-os3

#Create nodes
for {set i 1} {$i <= $maxnodes} {incr i} {
   set node($i) [$ns node]
   tb-set-node-os $node($i) $os
    
}

#Create duplex links and set loss rate
for {set i 1} {$i <  $maxnodes} {incr i} {	
    #    set iplus1 $i ; incr iplus1	
    set link($i) [$ns duplex-link $node($i) $node([expr $i + 1])   $bw $delay  $queue]
    tb-set-link-loss  $link($i) $loss

}
$ns rtproto $protocol
#Create a set of events that will bring the links up/down
#	$ns at 100.0 "$link0 down"
#	$ns at 110.0 "$link0 up"


## find length of  protos and nexthops


set runs [expr [llength $protos]*[llength $perhops] ]

set MAX_SIM_TIME [expr $runs*$finish + 2*$runs*$WARMUPTIME]
set ONE_CYCLE_LENGTH $finish + 2*$WARMUPTIME
set uplist {}
set downlist {}

# Parameters give by command line
set up $up
set down $down
# Time list when to fire up event
set uplist ""
# Time list when to fire down event
set downlist ""

if {$linkdynamics == 1} {
    for {set i 1} {$i <  $maxnodes} {incr i} {
    #Do stuff for the ith link
	set offset [expr $OFFSET_VAL*$i]
	sched $offset
	## Now you have uplist and downlist and use them to schedule your events
	
	set linkname "link-$i"
	foreach uptime $uplist {
	    $ns at $uptime "$linkname up"
	}
	foreach downtime $downlist {
	    $ns at $downtime "$linkname down"
	}
    }
}

### Node programs

for {set i 1} {$i <=  $maxnodes} {incr i} {
    set starttime $WARMUPTIME
     foreach proto $protos {
	foreach perhop $perhops {
	    set progVar "prog-$proto-$perhop-$i"
	    #    set [set progVar]  [new Program $ns]
	    set $progVar  [new Program $ns]
	    $progVar set node $node($i)
	    $progVar set command [get-cmd $i $perhop $proto]
	    set mytime $starttime
	    set end [expr  $starttime + $finish]
	    ## Source is later as it will contact server on socket
	    if {$i == 1} { set mytime [expr $starttime + $WARMUPTIME] }
	    $ns at  $mytime "[set $progVar] start"
	    $ns at  $end "[set $progVar] stop"
	    
	    set starttime [expr  $starttime + $finish + $WARMUPTIME]
	}

    }
}



$ns run
#$ns at 1800 "$ns swapout"


