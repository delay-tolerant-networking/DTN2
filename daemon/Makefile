#
# Makefile for DTN2/daemon
#
SRCDIR := ..

DAEMON_SRCS :=		\
	dtnd.cc		\
	../version.c

DAEMON_OBJS := $(DAEMON_SRCS:.cc=.o)

#
# Default target is to build the daemon
#
all: dtnd

COMPONENT_LIBS := \
	$(SRCDIR)/applib/libdtnapisrv.a \
	$(SRCDIR)/servlib/libdtnserv.a 	\
	$(SRCDIR)/oasys/liboasys.a 	\

dtnd: $(DAEMON_OBJS) $(COMPONENT_LIBS)
	$(CXX) $(DAEMON_OBJS) $(COMPONENT_LIBS) -o $@ $(LDFLAGS) $(LIBS)

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make
