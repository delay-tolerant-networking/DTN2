#
# Makefile for DTN2/sim
#
SRCDIR := ..


SIM_SRCS :=	\
	main.cc		\
	Event.cc	\
	Message.cc	\
	GlueNode.cc 	\
	SimpleNode.cc 	\
	SimulatorCommand.cc	\
	Simulator.cc	\
	SimContact.cc	\
	SimConvergenceLayer.cc \
	Simdtn2Command.cc \
	Topology.cc	\
	TrAgent.cc	\
	LogSim.cc	\

SIM_OBJS := $(SIM_SRCS:.cc=.o)

#
# Default target is to build the simulator
#
all: sim

sim: $(SIM_OBJS) $(SRCDIR)/servlib/libdtnserv.a
	$(CPP) $(SIM_OBJS) $(LINK_SERVLIB) $(STATIC_LIBS) -o $@ $(LFLAGS) $(LIBS)

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make
