# Start of Base Script to setup Emulab DTN2 experiments


set delay 10ms
set queue DropTail
set protocol Static



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
    
   set prefix /proj/DTN/nsdi/
#    set name "$prefix/logs/$exp.$protol.$perhopl.progobj"
#    set fd [open $name w]
#    puts $fd $args
#    close fd
#   return "$prefix/DTN2img/test/super-base-cmd.sh $name "
    return "$prefix/DTN2img/test/base-cmd.sh $args "
 
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


#tclsh foo.tcl -nodes 4 -proto "dtn mail tcp" -net "e2e hop"
#foreach {var val} $argv {
#    switch -- $var {
#	-nodes { set nodes $val  }
#	-proto { set protos $val }
#}
#if {[lindex $argv 3] == "-proto"} {
#    set protos [lindex $argv 4]
    


#set protos {}
#if {$dtn == 1}  { lappend protos "dtn" }
#if {$mail == 1} { lappend protos "mail" }
#if {$tcp == 1}  { lappend proto
#if {$proto == ""} { puts "Unidentified protocol" ; exit ; }

#Create program objects
# Depending upon perhop for each protocol 1 or 2 objects may be created

#if {$perhop > 2} {
#    puts "Value of perhop is $perhop . It should be 0,1 or 2" ; 
#    exit;
#}


set finish 700




###############################################################
## Please ensure that startime, stoptime, and proto are set correctly
###############################################################


for {set i 1} {$i <=  $maxnodes} {incr i} {
    set starttime 10
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
	    if {$i == 1} { set mytime [expr $starttime + 10] }
	    $ns at  $mytime "[set $progVar] start"
	    $ns at  $end "[set $progVar] stop"
	    
	    set starttime [expr  $starttime + $finish + 10]
	}

    }
}



$ns run
#$ns at 1800 "$ns swapout"


