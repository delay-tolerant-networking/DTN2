# Start of Base Script to setup Emulab DTN2 experiments



set delay 10ms
set queue DropTail
set protocol Static



## get cmd to be executed on node $id. 
proc get-cmd {id} {
    global proto
    global perhop
    global maxnodes
    global nfiles
    global size
    global exp
    set prefix /proj/DTN/nsdi/DTN2img/test
#    set  info /proj/DTN/exp/$exp/logs/info.$proto
    set args "$id $exp $maxnodes $nfiles $size $perhop"
    return "$prefix-$proto-cmd.sh $args"
}



# This is a simple ns script that demonstrates loops.
set ns [new Simulator]                  
source tb_compat.tcl
set os rhl-os1

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



#Create program objects



set starttime 10
set stoptime 1000

for {set i 1} {$i <=  $maxnodes} {incr i} {	
    set prog($i) [new Program $ns]
    $prog($i) set node $node($i)
    $prog($i) set command [get-cmd $i]

    set mytime $starttime
    ## Source has to be started later because it will try to contact server on socket
    if {$i == 1} { set mytime [expr $starttime + 10] }

    $ns at  $mytime "$prog($i) start"
    $ns at  $stoptime "$prog($i) stop"
        
}

$ns run



