#
# Makefile for DTN2/daemon
#
SRCDIR := ..

DAEMON_SRCS :=	\
	DTNCommands.cc \
	main.cc	\

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
	$(CPP) $(DAEMON_OBJS) $(COMPONENT_LIBS) \
		$(STATIC_LIBS) -o $@ $(LFLAGS) $(LIBS)

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make
