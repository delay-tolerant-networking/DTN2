#
# Makefile for DTN2/sim
#
SRCDIR := ..


SIM_SRCS :=	\
	dtnsim.cc	\
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
	FloodConsumer.cc\

#	Statistics.cc \

SIM_OBJS := $(SIM_SRCS:.cc=.o)

#
# Default target is to build the simulator
#
all: dtnsim

COMPONENT_LIBS := $(SRCDIR)/servlib/libdtnserv.a $(SRCDIR)/oasys/liboasys.a 
dtnsim: $(SIM_OBJS) $(COMPONENT_LIBS)
	$(CPP) $(SIM_OBJS) $(COMPONENT_LIBS) $(STATIC_LIBS) \
		-o $@ $(LFLAGS) $(LIBS)

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make
