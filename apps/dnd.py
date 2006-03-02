#!/usr/bin/python

# DTN Neighbor Discovery (over UDP Broadcast) -- A small python script
# that will propagate DTN registration information via UDP broadcasts.
#
# Written by Keith Scott, The MITRE Corporation

# I got tired of having to manually configure dtn daemons, expecially
# since the combination of the windows operating system and MITRE's
# dhcp/dynamic DNS caused machine names/addresses to change
# when least convenient.

# This script will populate the static routing tables of DTN daemons
# with registrations (and optionally routes) it hears from its peers.
# When advertising my local
# registrations, I append "/*" to the local EID and prune anything
# that would match on this route.  This way if I'm running something
# like bundlePing that generates 'transient' registrations,
# I don't end up cluttering up everybody else's tables with 'em.

# This script assumes that all machines use TCP convergence layers to
# communicate

# This script transmits UDP broadcast messages to a particular port
# (5005 by default).  You'll need to open up firewalls to let this
# traffic through.

# The UDP Messages sent are of the form:
#
#	my DTN local EID
#	my TCP CL Listen Port
#	registration/route 1
#	registration/route 2
#	...

# The myHostname used to be used by the receiver to instantiate (TCP)
# convergence-layer routes back to the sender.  It is now mainly just
# a tag to keep a sender from processing its own messages (receivers
# use the sender's IP address from received packets to set the CL
# destination for injected routes).

from socket import *
from time import *
import os
import random
import string
import thread
import re
import getopt
import sys

theBroadcastAddress = []
theBroadcastPort = 5005				# Port to which reg info is sent
dtnTclConsolePort = 5050			# Port on which DTN tcl interpreter is listening

broadcastInterval = 10 # How often to broadcast, in seconds

#
# Send a message to the dtn tcl interpreter and return results
#
#
def talktcl(sent):
	received = 0
	# print "Opening connection to dtnd tcl interpreter."
	sock = socket(AF_INET, SOCK_STREAM)
	try:
			sock.connect(("localhost", dtnTclConsolePort))
	except:
		print "Connection failed"
		sock.close()
		return None

	messlen, received = sock.send(sent), 0
	if messlen != len(sent):
		print "Failed to send complete message to tcl interpreter"
	else:
		# print "Message '",sent,"' sent to tcl interpreter."
		messlen = messlen

	data = ''
	while 1:
		promptsSeen = 0
		data += sock.recv(32)
		#sys.stdout.write(data)
		received += len(data)
		# print "Now received:", data
		# print "checking for '%' in received data stream [",received,"], ", len(data)
		for i in range(len(data)):
			if data[i]=='%':
				promptsSeen = promptsSeen + 1
			if promptsSeen>1:
				break;
		if promptsSeen>1:
			break;

	# print "talktcl received: ",data," back from tcl.\n"

	sock.close()
	return(data);
	
#
# Return the port on which the TCP convergence layer is listening
#
def findListeningPort():
	response = talktcl("interface list\n")
	if response==None:
		return None

	lines = string.split(response, "\n")
	for i in range(len(lines)):
		if string.find(lines[i], "Convergence Layer: tcp")>=0:
			words = string.split(lines[i+1])
			return(words[3])
	return None

#
# Munge the list 'lines' to contain only entries
# that contain (in the re.seach sense) at least
# one of the keys
#
def onlyLinesContaining(lines, keys):
	answer = []
	for theLine in lines:
		for theKey in keys:
			if re.search(theKey, theLine):
					answer += [theLine]
					break;
	return answer

#
# Generate a random string containing letters and digits
# of specified length
#
def generateRandom(length):
	chars = string.ascii_letters + string.digits
	return(''.join([random.choice(chars) for i in range(length)]))

#
# Generate a new unique link identifier
#
def genNewLink(linkList):
	done = False
	print "genNewLink: ", linkList
	while done==False:
		test = generateRandom(4)
		if len(linkList)>0:
			for i in range(len(linkList)):
				words = string.split(linkList[i], " ");
				if words[4]!=test:
					done = True
					break;
		else:
			done = True
	return test


#
# Return a pair of lists: the current links and the current
# routes from the DTN daemon
#
def getLinksRoutes():
	myRoutes = talktcl("route dump\n")
	if myRoutes==None:
		print "tryAddRoute: can't talk to dtn daemon"
		return([[],[]])
	myRoutes = string.strip(myRoutes, "dtn% ")

	# Split the response into lines
	lines = string.split(myRoutes, '\n');
	
	theRoutes = []
	theLinks = []

	# Find the routes
	for i in range(1,len(lines)):
		if string.find(lines[i], "Links")>=0:
			break
		else:
			if lines[i]=="\r":
				i = i
			else:
				theRoutes += [lines[i]]

	# Find the links
	if len(lines)>i+2:
		for j in range(i+1, len(lines)-2):
			theLinks += [lines[j]]

	return([theLinks, theRoutes])

# Return the link name of an existing link, or None
def alreadyHaveLink(theLinks, newLink):
	for testLink in theLinks:
		# This isn't really right, but it's close
		if string.find(testLink, newLink)>=0:
			testLink = string.split(testLink)
			return testLink[1]
	return None

def myBroadcast():
	answer = []
	myaddrs = os.popen("ip addr show").read()
	myaddrs = string.split(myaddrs, "\n")

	myaddrs = onlyLinesContaining(myaddrs, ["inet.*brd"])

	for addr in myaddrs:
		words = string.split(addr)
		for i in range(len(words)):
			if words[i]=="brd":
				answer += [words[i+1]]

	return answer

#
# Try to add a route to eid via tcp CL host:port
# Don't add if we've already got a matching route for
# the eid
#
def tryAddRoute(host, port, eid):
	# print "tryAddRoute(",host,",",port,",",eid,")"

	myRegistrations = getRegistrationList()
	theLinks, theRoutes = getLinksRoutes()

	# print "About to check eid "+eid+" against existing routes [",len(theRoutes),"]"
	if len(theRoutes)>0:
		for i in range(len(theRoutes)):
			theRoutes[i] = theRoutes[i].strip()
			nextHop = string.split(theRoutes[i])[0];
			# print "Checking",eid," against existing route:", nextHop
			foo = re.search(nextHop, eid)
			if foo==None:
				foo = foo
			else:
			#if theRoutes[i].find(eid)>=0:
				#print "Existing route found, not adding route for: "+eid
				return

	# print "About to check eid "+eid+" against my registrations."
	for myReg in myRegistrations:
		if string.find(myReg+"/*", eid)>=0:
			return

	# OK, we need to add a new route entry.  Start by getting
	# a new unique link name
	print "Adding new link/route"
	print theLinks
	print theRoutes

	# See if there's an existing link we can glom onto
	linkName = alreadyHaveLink(theLinks, host+":"+port)
	if linkName==None:
		linkName = genNewLink(theLinks)
	else:
		print "Adding route to existing link:", linkName

	# link add linkName host:port ONDEMAND tcp
	print "link add ",linkName," ",host+":"+port," ONDEMAND tcp"
	talktcl("link add "+linkName+" "+host+":"+port+" ONDEMAND tcp\n")
	
	# route add EID linkName
	print "route add",eid," ",linkName
	talktcl("route add "+eid+" "+linkName+"\n")
	return

#
# Server Thread
#
def doServer(host, port):
# Set the socket parameters
	buf = 1024
	addr = (host,port)

	print "doServer started on host:", host, "port: ", port

	# Create socket and bind to address
	UDPSock = socket(AF_INET,SOCK_DGRAM)
	UDPSock.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
	UDPSock.bind(addr)

	myEID = myLocalEID()

	# Receive messages
	while 1:
		data,addr = UDPSock.recvfrom(buf)
		if not data:
			print "Client has exited!"
			break
		else:
			print "\nReceived message '", data,"' from addr:", addr
			things = string.split(data, '\n')
			# Am I the sender of this message?
			if things[0] == myEID:
				print "I don't process my own messages (",things[0],",",gethostname(),")"
				continue
			# For each registration in the message, see if I've
			# already got one and if not, add a route
			for i in range(2, len(things)-1):
				tryAddRoute(addr[0], things[1], things[i])  # host, port, EID

	# Close socket
	UDPSock.close()

#
# Return a list of strings that are the current
# registrations
#
def getRegistrationList():
	response = talktcl("registration list\n")
	if response==None:
		return(())
	response = string.strip(response, "dtn% ")

	# Split the response into lines
	lines = string.split(response, '\n');

	# Throw away things that are not registrations
	lines = onlyLinesContaining(lines, ["id "])
	answer = []
	for i in range(len(lines)):
			temp = string.split(lines[i], " ")
			answer += [temp[3]]
	return answer

#
# return my local EID
#
def myLocalEID():
	foo = talktcl("registration dump\n");
	if foo==None:
		return None

	foo = string.strip(foo, "dtn% ")
	foo = string.split(foo, "\n");
	foo = onlyLinesContaining(foo, "id 0:")
	foo = string.split(foo[0])
	return foo[3]
	
def alreadyCovered(list, newItem):
	for listItem in list:
			if re.search(newItem, listItem):
					return(True)
	return False

def doClient(sendToAddresses, port):
	print "doClient thread started with sendToAddresses:", sendToAddresses, ", port:", port

	# Create socket
	UDPSock = socket(AF_INET,SOCK_DGRAM)
	UDPSock.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)

	myListenPort = findListeningPort()
	if myListenPort == None:
		print "Can't find listening port for TCP CL, client exiting."
		return

	myEID = myLocalEID()
	if myEID==None:
		print "Can't get local EID.  exiting"
		sys.exit(-1)
		
	print "client local EID is:", myEID

	# Send messages
	while (1):
		thingsSent = []
		theList = getRegistrationList();
		print "getRegistrationList() returned:", theList

		if len(theList)==0:
			print "Can't find registrations"
		else:
			# Build a message that contains my IP address and port,
			# plus the list of registrations
			thingsSent += [myEID+"/*"]
			for listEntry in theList:
				if alreadyCovered(thingsSent, listEntry):
					continue
				else:
					thingsSent += [listEntry]

			if True:
				theLinks, theRoutes = getLinksRoutes()
				for route in theRoutes:
					route = string.split(route)
					route = route[0]
					if alreadyCovered(thingsSent, route):
						continue
					else:
						thingsSent += [route]

			# Now build the text string to send
			msg = myEID+'\n'
			msg += myListenPort
			msg += '\n'
			for entry in thingsSent:
					msg += entry
					msg += "\n"
			print "msg to send is:",msg

			# Send to desired addresses
			for addr in sendToAddresses:
					try:
						if(UDPSock.sendto(msg,(addr, port))):
							msg = msg
					except:
						print "Error sending message to:", addr
						print os.strerror()
		sleep(broadcastInterval)

	# Close socket
	UDPSock.close()


def usage():
	print "dnd.py [-h] [-s] [-c] [-b PORT] [-t PORT] [-r] [addr] [addr...]"
	print "  -h:   Print usage information (this)"
	print "  -s:   Only perform server (receiving) actions"
	print "  -c:   Only perform client (transmitting) actions"
	print "  -b #: Set the port for dnd UDP messaging ("+str(theBroadcastPort)+")"
	print "  -t #: Set the DTN Tcl Console Port ("+str(dtnTclConsolePort)+")"
#	print "  -d addr: Add a destination addr to transmit info to"
#	print "           (subnet broadcasts work well here)"
#	print "           Current default is:", myBroadcast()
	print "  -r:   Include route information in addition to (local) registration"
	print "           information.  This makes neighbor discovery into a"
	print "           really stupid routing algorithm, but possibly suitable"
	print "           for small lab setups (like several dtn routes in a"
	print "           linear topology).  Note that this is NOT EVEN as"
	print "           sophisticated as RIP (there's no 'distance')."
	print "  addrs are addresses to which UDP packets should be sent"
	print "        default:", myBroadcast()

if __name__ == '__main__':
	print "argv is:", sys.argv, "[", len(sys.argv), "]"
	serverOn = True
	clientOn = True
	
	try:
			opts, args = getopt.getopt(sys.argv[1:], "d:b:t:hsc", ["help", "server", "client"])
	except getopt.GetoptError:
		usage()
		sys.exit(2)

	for o, a in opts:
		if o == "-h":
			usage();
			sys.exit(2)
		if o == "-s":
			clientOn = False
		if o == "-d":
			theBroadcastAddress += [a]
		if o == "-c":
			serverOn = False
		if o == "-b":
			theBroadcastPort = a
		if o == "-t":
			dtnTclConsolePort = a

	print "rest of args is now:", args

	if len(theBroadcastAddress)==0:
		#theBroadcastAddress = ['<broadcast>']
		theBroadcastAddress = myBroadcast()
		print "I figure to transmit to:", theBroadcastAddress

	if clientOn:
		thread.start_new(doClient, (theBroadcastAddress, theBroadcastPort))
	if serverOn:
		thread.start_new(doServer, ("", theBroadcastPort))

	# Now I just sort of hang out...
	while 1:
		sleep(10)

