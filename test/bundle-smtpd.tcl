#
# test script that runs an smtp daemon within the bundle daemon
#

package require smtp
package require smtpd
package require mime

proc smtpd_deliver_mime {token} {
    set sender [lindex [mime::getheader $token From] 0]
    set recipients [lindex [mime::getheader $token To] 0]
    set mail "From $sender [clock format [clock seconds]]"
    append mail "\n" [mime::buildmessage $token]
    puts $mail
    
    # mail inject $to $body
}

proc smtpd_start {port {deliver_mime smtpd_deliver_mime}} {
    smtpd::configure -deliverMIME $deliver_mime
    smtpd::start 0 $port
}
    
proc send_test_mail {} {
    global smtp_port
    
    set token [mime::initialize -canonical text/plain \
	    -string "this is a test"]
    mime::setheader $token To "test@test.com"
    mime::setheader $token Subject "test subject"
    smtp::sendmessage $token \
	    -recipients test@test.com -servers localhost -ports $smtp_port
    mime::finalize $token
}
