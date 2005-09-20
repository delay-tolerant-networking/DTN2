#
# Makefile for DTN2/apps/dtncp
#

SRCDIR := ../..

all: dtnperf-client dtnperf-server

dtnperf-client: dtnperf-client.o
	$(CC) $< $(APPLFLAGS) -o $@

dtnperf-server: dtnperf-server.o
	$(CC) $< $(APPLFLAGS) -o $@

-include ../Makefile.app
