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
	timer-test		\


test: check $(BINFILES)

#
# Include the servlib Makefile to get the list of servlib objects
#
SERVLIB_MAKEFILE_INCLUDE := true
-include $(SRCDIR)/servlib/Makefile
SERVLIB_OBJS := $(patsubst %,$(SRCDIR)/servlib/%,$(SERVLIB_OBJS))

echoobjs:
	@echo $(SERVLIB_OBJS)

#
# Default pattern rule to build the tests
#
%-test.o: %-test.cc
	$(CPP) $(CFLAGS) -c $< -o $@

%-test: %-test.o $(SERVLIB_OBJS)
	$(CPP) $(CFLAGS) $< $(SERVLIB_OBJS) -o $@ $(LFLAGS) $(LIBS) 

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make
