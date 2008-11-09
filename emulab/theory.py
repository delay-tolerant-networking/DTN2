#!/usr/bin/python

import sys
from heapq import *
from optparse import OptionParser

def dprint(msg):
    print "[%d]: %s" % (time, msg)
    
arglist = ("count", "size", "num_hops", "bw", "hop_mode", "conn", "uptime", "downtime")

usage = "usage: %prog [options]"
parser = OptionParser(usage)
parser.add_option('--size', type='int', help='size of each message')
parser.add_option('--count', type='int', help='number of messages')
parser.add_option('--num_hops', type='int', help='number of hops')
parser.add_option('--bw', type='int', help='bandwidth')
parser.add_option('--hop_mode', help='hop mode (hop or e2e)')
parser.add_option('--conn', help='connectivity mode')
parser.add_option('--uptime', type='int', help='uptime in seconds')
parser.add_option('--downtime', type='int', help='downtime in seconds')

parser.set_defaults(num_hops=5)
parser.set_defaults(uptime=60)
parser.set_defaults(downtime=240)

(opts, args) = parser.parse_args()

def die():
    parser.print_help()
    sys.exit(0)

if opts.count    == None or \
   opts.size     == None or \
   opts.num_hops == None or \
   opts.bw       == None or \
   opts.hop_mode == None or \
   opts.conn     == None or \
   opts.uptime   == None or \
   opts.downtime == None: die()

count    = opts.count
size     = opts.size
num_hops = opts.num_hops
bw       = opts.bw
uptime   = opts.uptime
downtime = opts.downtime
hop_mode = opts.hop_mode
conn     = opts.conn

last = num_hops - 1
total_size = count * size * 8

amount = map(lambda x: 0, range(0, num_hops))
links  = map(lambda x: True, range(0, num_hops))
amount[0] = total_size
q = []

class SimDoneEvent:
    def __init__(self, time):
        self.time = time

    def run(self):
        print "maximum simulation time (%d) reached... ending simulation" % self.time
        sys.exit(1)
        
class LinkEvent:
    def __init__(self, time, link, mode):
        self.time = time
        self.link = link
        self.mode = mode
        
    def __cmp__(self, other):
        return self.time.__cmp__(other.time)

    def __str__(self):
        return "Event @%d: link %d %s" % (self.time, self.link, self.mode)

    def run(self):
        if self.mode == 'up':
            dprint('opening link %d' % self.link)
            links[self.link] = True
            self.time += uptime
            self.mode = 'down'
        else:
            dprint('closing link %d' % self.link)
            links[self.link] = False
            self.time += downtime
            self.mode = 'up'

        queue_event(self)

class CompletedEvent:
    def __init__(self, time, node):
        self.time = time
        self.node = node

    def run(self):
        pass

def queue_event(e):
    global q
#    dprint('queuing event %s' % e)
    heappush(q, e)

# simulator completion event
time = 0
queue_event(SimDoneEvent(60*30))

# initial link events
if (conn == 'conn'):
    pass

elif (conn == 'all2'):
    for i in range(1, num_hops):
        queue_event(LinkEvent(uptime, i, 'down'))

elif (conn == 'sequential'):
    queue_event(LinkEvent(uptime, 1, 'down'))
    for i in range(2, num_hops):
        links[i] = False
        queue_event(LinkEvent((i-1) * 60, i, 'up'))

elif (conn == 'offset2'):
    for i in range (1, num_hops):
        if i % 2 == 0:
            links[i] = False
            queue_event(LinkEvent(120, i, 'up'))
        else:
            queue_event(LinkEvent(uptime, i, 'down'))

elif (conn == 'shift10'):
    if num_hops * 10 > 60:
        raise(ValueError("can't handle more than 6 hops"))

    queue_event(LinkEvent(uptime, 1, 'down'))
    for i in range (2, num_hops):
        links[i] = False
        queue_event(LinkEvent(10 * (i-1), i, 'up'))

else:
    raise(ValueError("conn mode %s not defined" % conn))

print 'initial link states:'
for i in range(0, num_hops):
    print '\t%d: %s' % (i, links[i])


def can_move(i):
    if hop_mode == 'hop':
        dest = i+1
        hops = (i+1,)
    else:
        dest = last
        hops = range(i+1, last+1)
        
    for j in hops:
        if links[j] != True:
#            dprint("can't move data from %d to %d since link %d closed" % (i, dest, j))
            return False
    return True
    
# proc to shuffle a given amount of data through the network
def move_data(interval):
    dprint('%d seconds elapsed... trying to move data' % interval)
    
    for i in range(0, last):
        if not can_move(i):
            continue

        if hop_mode == 'hop':
            dest = i+1
        else:
            dest = last
        
        amt = min(amount[i], interval * bw)
        
        if (amt != 0):
            dprint('moving %d/%d bits (%d msgs) from %d to %d' %
                   (amt, amount[i], amt / (size*8), i, dest))
            amount[i]    -= amt
            amount[dest] += amt

        if dest == last and amount[dest] == total_size:
            print "all data transferred..."
            print "ELAPSED %d" % (time + interval)
            sys.exit(0)

def blocked():
    for i in range(0, last):
        if can_move(i):
            return False
    return True

def completion_time():
    # if nothing can move, then we have infinite completion time
    if blocked():
        return 9999999999.0
    
    return float(sum(amount[:-1])) / float(bw)
    
while True:
    try:
        next_event = heappop(q)
    except:
        raise RuntimeError('no events in queue but not complete')

    tcomplete = completion_time()
    elapsed = next_event.time - time
    if (tcomplete < elapsed):
        dprint('trying to move last chunk')
        move_data(tcomplete)

    time = next_event.time
    if (elapsed != 0 and not blocked()):
        move_data(elapsed)

    next_event.run()

