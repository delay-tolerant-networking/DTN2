#
# Makefile for DTN2/sim
#
SRCDIR := ..


OLD_SIM_SRCS :=	\
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

SIM_SRCS := \
	dtnsim.cc	\
	LogSim.cc	\
	Node.cc		\
	Simulator.cc	\
	SimCommand.cc	\
	Topology.cc	\

SIM_OBJS := $(SIM_SRCS:.cc=.o)

#
# Default target is to build the simulator
#
all: dtnsim

COMPONENT_LIBS := $(SRCDIR)/servlib/libdtnserv.a $(SRCDIR)/oasys/liboasys.a 
dtnsim: $(SIM_OBJS) $(COMPONENT_LIBS)
	$(CXX) $(SIM_OBJS) $(COMPONENT_LIBS) -o $@ $(LDFLAGS) $(LIBS)

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make
