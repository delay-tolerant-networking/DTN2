#
# Makefile for DTN2/test
#

SRCDIR := ..

#
# Default target is to build all tests
#
all: test

test: simple-marshal-test

#
# Default rule to build a test
simple-marshal-test: simple-marshal-test.o $(SRCDIR)/servlib/libdtnserv.a
	$(CPP) $^ -o $@ $(LFLAGS) $(LIBS)

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make


