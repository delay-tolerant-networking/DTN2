#
# Makefile for DTN2/daemon
#
SRCDIR := ..

DAEMON_SRCS :=	\
	main.cc	\

DAEMON_OBJS := $(DAEMON_SRCS:.cc=.o)

#
# Default target is to build the daemon
#
all: bundleDaemon

bundleDaemon: $(DAEMON_OBJS) $(SRCDIR)/servlib/libdtnserv.a
	$(CPP) $(DAEMON_OBJS) $(LINK_SERVLIB) -o $@ $(LFLAGS) $(LIBS)

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make

