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

dtnd: $(DAEMON_OBJS) $(SRCDIR)/servlib/libdtnserv.a $(SRCDIR)/applib/libdtnapisrv.a
	$(CPP) $(DAEMON_OBJS) $(LINK_SERVLIB) $(SRCDIR)/applib/libdtnapisrv.a\
		$(STATIC_LIBS) -o $@ $(LFLAGS) $(LIBS)

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make
