#
# Makefile for DTN2/test
#
SRCDIR := ..

#
# Default target is to build all tests
#
all: test

BINFILES :=			\
	command-test		\
	simple-marshal-test	\
	simple-sql-test		\


test: check $(BINFILES)

#
# Default pattern rule to build the tests
#
%-test: %-test.cc $(SRCDIR)/servlib/libdtnserv.a
	$(CPP) $(CFLAGS) $< $(LINK_SERVLIB) -o $@ $(LFLAGS) $(LIBS) 

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make
