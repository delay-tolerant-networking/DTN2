
link add tcp0 host://pisco.cs.berkeley.edu:10000 ONDEMAND tcp

for {set i 0} {$i < 10000} {incr i 100} {
    log /test debug "TEST delay $i"
    link open tcp0
    after $i
    link close tcp0
}
