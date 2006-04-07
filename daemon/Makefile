#
# Makefile for DTN2/daemon
#

#
# Make sure SRCDIR is set (.. by default)
#
ifeq ($(SRCDIR),)
SRCDIR   := ..
BUILDDIR := ..
endif

DAEMON_SRCS :=			\
	dtnd.cc

DAEMON_OBJS := $(DAEMON_SRCS:.cc=.o)

#
# Default target is to build the daemon
#
BINFILES := dtnd
all: $(BINFILES)

COMPONENT_LIBS := \
	../applib/libdtnapisrv.a 	\
	../servlib/libdtnserv.a 	\
	../oasys/liboasys.a 		\

dtnd: $(DAEMON_OBJS) $(COMPONENT_LIBS)
	$(CXX) $(DAEMON_OBJS) $(COMPONENT_LIBS) \
		-o $@ $(LDFLAGS) $(LIBS) $(OASYS_LIBS)

#
# Include the common rules
#
include ../Rules.make

