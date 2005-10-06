#
# Makefile for DTN2/test
#

#
# Make sure SRCDIR is set.
#
ifeq ($(SRCDIR),)
SRCDIR := ../..
CFGDIR := ../..
endif

#
# Default target is to build all unit tests
#
all: unit_tests

BINFILES :=				\
	unit_tests/bundle-list-test	\
	unit_tests/bundle-payload-test	\
	unit_tests/bundle-protocol-test	\
	unit_tests/bundle-timestamp-test\
	unit_tests/endpoint-id-test	\
	unit_tests/sdnv-test		\

unit_tests: $(BINFILES)

#
# Include the servlib Makefile to get the list of servlib objects
#
SERVLIB_MAKEFILE_INCLUDE := true
-include $(SRCDIR)/servlib/Makefile
SERVLIB_OBJS := $(patsubst %,$(SRCDIR)/servlib/%,$(SERVLIB_OBJS))

echoobjs:
	@echo $(SERVLIB_OBJS)

COMPONENT_LIBS := \
	$(SRCDIR)/applib/libdtnapisrv.a \
	$(SRCDIR)/servlib/libdtnserv.a 	\
	$(SRCDIR)/oasys/liboasys.a 	\

#
# Default pattern rule to build the tests
#
unit_tests/%-test.o: unit_tests/%-test.cc
	$(CXX) $(CFLAGS) -c $< -o $@

%-test: unit_tests/%-test
unit_tests/%-test: unit_tests/%-test.o $(COMPONENT_LIBS)
	$(CXX) $(CFLAGS) $< $(COMPONENT_LIBS) \
		-o $@ $(LDFLAGS) $(LIBS) 


#
# Include the common rules
#
-include $(SRCDIR)/Rules.make
