#
# Makefile for DTN2/test
#
SRCDIR := ..

#
# Default target is to build all tests
#
all: test

BINFILES :=			\
	address-family-test	\
	timer-test		\

BROKEN := 			\
	command-test		\
	simple-marshal-test	\
	simple-sql-test		\


test: $(BINFILES)

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

%-test: %-test.o $(SERVLIB_OBJS) $(SRCDIR)/oasys/liboasys.a
	$(CPP) $(CFLAGS) $< $(SERVLIB_OBJS) $(SRCDIR)/oasys/liboasys.a \
		-o $@ $(LFLAGS) $(LIBS) 

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make
