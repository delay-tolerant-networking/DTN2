#
#    Copyright 2004-2006 Intel Corporation
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

#
# Makefile for DTN2/servlib
#

#
# Source and object rules
#

BUNDLING_SRCS := 				\
        bundling/APIBlockProcessor.cc           \
	bundling/BlockInfo.cc			\
	bundling/BlockProcessor.cc		\
	bundling/Bundle.cc			\
	bundling/BundleActions.cc		\
	bundling/BundleDaemon.cc		\
	bundling/BundleEvent.cc			\
	bundling/BundleEventHandler.cc		\
	bundling/BundleList.cc			\
	bundling/BundlePayload.cc		\
	bundling/BundleProtocol.cc		\
	bundling/BundleStatusReport.cc		\
	bundling/BundleTimestamp.cc		\
	bundling/CustodySignal.cc		\
	bundling/CustodyTimer.cc		\
	bundling/ForwardingInfo.cc		\
	bundling/ForwardingLog.cc		\
	bundling/FragmentManager.cc		\
	bundling/ExpirationTimer.cc		\
	bundling/GbofId.cc  		        \
	bundling/PayloadBlockProcessor.cc	\
	bundling/PreviousHopBlockProcessor.cc	\
	bundling/PrimaryBlockProcessor.cc	\
	bundling/SDNV.cc			\
	bundling/UnknownBlockProcessor.cc	\

CONTACT_SRCS :=					\
	contacts/AlwaysOnLink.cc		\
	contacts/Contact.cc			\
	contacts/ContactManager.cc		\
	contacts/Interface.cc			\
	contacts/InterfaceTable.cc		\
	contacts/Link.cc			\
	contacts/OndemandLink.cc		\
	contacts/OpportunisticLink.cc		\
	contacts/ScheduledLink.cc		\

CONV_LAYER_SRCS :=				\
	conv_layers/BluetoothConvergenceLayer.cc  \
	conv_layers/ConnectionConvergenceLayer.cc \
	conv_layers/CLConnection.cc 		\
	conv_layers/ConvergenceLayer.cc		\
	conv_layers/EthConvergenceLayer.cc	\
	conv_layers/FileConvergenceLayer.cc	\
	conv_layers/IPConvergenceLayer.cc	\
	conv_layers/IPConvergenceLayerUtils.cc	\
	conv_layers/NullConvergenceLayer.cc	\
	conv_layers/StreamConvergenceLayer.cc   \
	conv_layers/TCPConvergenceLayer.cc	\
	conv_layers/UDPConvergenceLayer.cc	\
	conv_layers/ExternalConvergenceLayer.cc	\
	conv_layers/ECLModule.cc \
	conv_layers/CLEventHandler.cc \
	conv_layers/clevent.cc \

DISCOVERY_SRCS :=				\
	discovery/Announce.cc			\
	discovery/BluetoothAnnounce.cc		\
	discovery/BluetoothDiscovery.cc		\
	discovery/BonjourDiscovery.cc		\
	discovery/Discovery.cc			\
	discovery/DiscoveryTable.cc		\
	discovery/IPAnnounce.cc			\
	discovery/IPDiscovery.cc		\

CMD_SRCS :=					\
	cmd/APICommand.cc			\
	cmd/BundleCommand.cc			\
	cmd/CompletionNotifier.cc		\
	cmd/InterfaceCommand.cc			\
	cmd/LinkCommand.cc          		\
	cmd/ParamCommand.cc			\
	cmd/RegistrationCommand.cc		\
	cmd/RouteCommand.cc			\
	cmd/ShutdownCommand.cc			\
	cmd/StorageCommand.cc			\
	cmd/TestCommand.cc			\
	cmd/DiscoveryCommand.cc			\
	cmd/ProphetCommand.cc			\
	cmd/ECLACommand.cc			\

NAMING_SRCS :=					\
	naming/EndpointID.cc			\
	naming/EndpointIDOpt.cc			\
	naming/Scheme.cc			\
	naming/SchemeTable.cc			\
	naming/DTNScheme.cc			\
	naming/EthernetScheme.cc		\
	naming/StringScheme.cc			\
	naming/WildcardScheme.cc		\
	naming/TCAScheme.cc			\

REGISTRATION_SRCS :=				\
	reg/AdminRegistration.cc		\
	reg/APIRegistration.cc			\
	reg/Registration.cc			\
	reg/RegistrationTable.cc		\
	reg/LoggingRegistration.cc		\
	reg/PingRegistration.cc			\
	reg/TclRegistration.cc			\

ROUTING_SRCS :=					\
	routing/router.cc			\
	routing/router-custom.cc		\
	routing/DuplicateCache.cc		\
	routing/ExternalRouter.cc		\
	routing/BundleRouter.cc			\
	routing/FloodBundleRouter.cc		\
	routing/NeighborhoodRouter.cc		\
	routing/LinkStateGraph.cc		\
	routing/LinkStateRouter.cc		\
	routing/LinkScheduleEstimator.cc	\
	routing/Prophet.cc			\
	routing/ProphetController.cc		\
	routing/ProphetEncounter.cc		\
	routing/ProphetLists.cc			\
	routing/ProphetNode.cc			\
	routing/ProphetRouter.cc		\
	routing/ProphetTLV.cc			\
	routing/RouteEntry.cc			\
	routing/RouteTable.cc			\
	routing/ShortestPathRouter.cc		\
	routing/TableBasedRouter.cc		\
	routing/TcaEndpointID.cc                \
	routing/TcaControlBundle.cc             \
	routing/TcaRouter.cc                    \

OLD_STORAGE_SRCS :=				\
	storage/GlobalStore.cc			\
	storage/PersistentStore.cc		\
	storage/StorageConfig.cc		\
	storage/BerkeleyDBStore.cc		\
	storage/SQLStore.cc			\
        storage/PostgresSQLImplementation.cc    \
        storage/MysqlSQLImplementation.cc       \

STORAGE_SRCS :=					\
	storage/BundleStore.cc			\
	storage/ProphetStore.cc			\
	storage/LinkStore.cc			\
	storage/GlobalStore.cc			\
	storage/RegistrationStore.cc		\

SERVLIB_SRCS := 				\
	DTNServer.cc				\
	$(BUNDLING_SRCS) 			\
	$(CONTACT_SRCS)				\
	$(CONV_LAYER_SRCS)			\
	$(DISCOVERY_SRCS)			\
	$(CMD_SRCS) 				\
	$(NAMING_SRCS) 				\
	$(REGISTRATION_SRCS)			\
	$(ROUTING_SRCS)				\
	$(STORAGE_SRCS)				\

SERVLIB_OBJS := $(SERVLIB_SRCS:.cc=.o)
SERVLIB_OBJS := $(SERVLIB_OBJS:.c=.o)
SERVLIB_OBJS := $(SERVLIB_OBJS) ../dtn-version.o

ALLSRCS := $(SERVLIB_SRCS)

#
# Other makefiles include this one so they can link with the object
# files directly and not use the servlib, but in that case, all we
# want are the object lists, not the rules below
#
ifeq ($(SERVLIB_MAKEFILE_INCLUDE),)

#
# Default target is to build the library
#
LIBFILES := libdtnserv.a
all: $(LIBFILES)

servlib: libdtnserv.a
libdtnserv.a: $(SERVLIB_OBJS)
	rm -f $@
	$(AR) ruc $@ $^
	$(RANLIB) $@ || true

#
# Need special rules for the gdtoa sources adapted from the source
# distribution.
#
debug/arith-native.h: debug/gdtoa-arithchk.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o debug/arithchk
	debug/arithchk > $@
	rm -f debug/arithchk

debug/arith.h:
	$(MAKE) debug/arith-$(TARGET).h
	cp debug/arith-$(TARGET).h $@

debug/Formatter.cc: debug/arith.h

debug/gdtoa-%.o: debug/gdtoa-%.c debug/arith.h
	$(CC) -DINFNAN_CHECK $(CPPFLAGS) $(CFLAGS) -c $< -o $@

GENFILES += debug/arith.h debug/arith-native.h debug/arithchk

#
# Make sure SRCDIR is set (.. by default)
#
ifeq ($(SRCDIR),)
SRCDIR   := ..
BUILDDIR := ..
endif

#
# Include the common rules.
#
include ../Rules.make

#
# Rules to regenerate XML bindings if the 'xsd' tool is present.
#

# Arguments used to generate both the ECLA and external router bindings.
XSD_TOOL_COMMON_ARGS := \
	--generate-serialization \
	--generate-default-ctor \
	--morph-anonymous \
	--hxx-suffix .h \
	--cxx-suffix .cc \

# Arguments used to regenerate the ECLA bindings.
XSD_TOOL_CLA_ARGS := \
	--root-element cl_message \
	--namespace-map =dtn::clmessage \
	--output-dir conv_layers \
	--cxx-prologue '\#ifdef HAVE_CONFIG_H' \
	--cxx-prologue '\#  include <config.h>' \
	--cxx-prologue '\#endif' \
	--cxx-prologue '\#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_CL_ENABLED)' \
	--cxx-epilogue '\#endif' \

# Arguments used to regenerate the external router bindings.
XSD_TOOL_RTR_ARGS := \
	--root-element bpa \
	--namespace-map =dtn::rtrmessage \
	--output-dir routing \
	--custom-type linkType=/linkType_base \
	--custom-type bundleType=/bundleType_base \
	--custom-type contactType=/contactType_base \
	--custom-type eidType=/eidType_base \
	--custom-type gbofIdType=/gbofIdType_base \
	--custom-type key_value_pair=/key_value_pair_base \
	--custom-type routeEntryType=/routeEntryType_base \
	--custom-type registrationType=/registrationType_base \
	--hxx-epilogue '\#include "router-custom.h"' \
	--cxx-prologue '\#ifdef HAVE_CONFIG_H' \
	--cxx-prologue '\#  include <config.h>' \
	--cxx-prologue '\#endif' \
	--cxx-prologue '\#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)' \
	--cxx-epilogue '\#endif' \

# Regenerate XML bindings. 
xsdbindings: conv_layers/clevent.xsd routing/router.xsd
ifdef XSD_TOOL
	$(XSD_TOOL) cxx-tree $(XSD_TOOL_COMMON_ARGS) $(XSD_TOOL_CLA_ARGS) conv_layers/clevent.xsd
	$(XSD_TOOL) cxx-tree $(XSD_TOOL_COMMON_ARGS) $(XSD_TOOL_RTR_ARGS) routing/router.xsd
else
	@echo "ERROR:   configure was unable to find the xsd tool needed to"
	@echo "         regenerate the XML bindings."
	@echo "         Use the --with-xsd-tool=(name) option with configure"
	@echo "         specify the location of this tool."
endif

endif
