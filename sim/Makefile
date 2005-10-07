#
# Makefile for DTN2/sim
#

#
# Make sure SRCDIR is set (.. by default)
#
ifeq ($(SRCDIR),)
SRCDIR   := ..
BUILDDIR := ..
endif

OLD_SIM_SRCS :=	\
	dtnsim.cc	\
	Event.cc	\
	Message.cc	\
	GlueNode.cc 	\
	SimpleNode.cc 	\
	SimulatorCommand.cc	\
	Simulator.cc	\
	SimContact.cc	\
	Simdtn2Command.cc \
	Topology.cc	\
	TrAgent.cc	\
	LogSim.cc	\
	FloodConsumer.cc\

#	Statistics.cc \

SIM_SRCS := \
	dtnsim.cc		\
	Connectivity.cc		\
	ConnCommand.cc		\
	Node.cc			\
	NodeCommand.cc		\
	Simulator.cc		\
	SimBundleActions.cc	\
	SimCommand.cc		\
	SimConvergenceLayer.cc	\
	SimRegistration.cc	\
	Topology.cc		\
	TrAgent.cc		\

SIM_OBJS := $(SIM_SRCS:.cc=.o)

#
# Default target is to build the simulator
#
all: dtnsim

#
# Include dtnsim in files to clean with make clean
#
BINFILES += dtnsim

COMPONENT_LIBS := ../servlib/libdtnserv.a ../oasys/liboasys.a 

dtnsim: $(SIM_OBJS) $(COMPONENT_LIBS)
	$(CXX) $(SIM_OBJS) $(COMPONENT_LIBS) -o $@ $(LDFLAGS) $(LIBS)

#
# Include the common rules
#
-include ../Rules.make
