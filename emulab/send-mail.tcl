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

proc usage {} {
    puts "usage: send-mail <file> <copies> <destination>"
    exit 1
}

if {[llength $argv] != 3} {
    usage
}

set file   [lindex $argv 0]
set copies [lindex $argv 1]
set dest   [lindex $argv 2]

for {set i 0} {$i < $copies} {incr i} {
    exec mail -s "TEST $i" $dest < $file
}
