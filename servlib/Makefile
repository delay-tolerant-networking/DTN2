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
	bundling/AnnounceBundle.cc		\
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
	bundling/PayloadBlockProcessor.cc	\
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
	routing/ExternalRouter.cc		\
	routing/BundleRouter.cc			\
	routing/FloodBundleRouter.cc		\
	routing/NeighborhoodRouter.cc		\
	routing/ProphetRouter.cc		\
	routing/LinkStateGraph.cc		\
	routing/LinkStateRouter.cc		\
	routing/LinkScheduleEstimator.cc	\
	routing/RouteEntry.cc			\
	routing/RouteTable.cc			\
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
	storage/LinkStore.cc			\
	storage/GlobalStore.cc			\
	storage/RegistrationStore.cc		\

SERVLIB_SRCS := 				\
	DTNServer.cc				\
	$(BUNDLING_SRCS) 			\
	$(CONTACT_SRCS)				\
	$(CONV_LAYER_SRCS)			\
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
	$(CC) $(DEBUG) $(OPTIMIZE) $< -o debug/arithchk
	debug/arithchk > $@
	rm -f debug/arithchk

debug/arith.h:
	$(MAKE) debug/arith-$(TARGET).h
	cp debug/arith-$(TARGET).h $@

debug/Formatter.cc: debug/arith.h

debug/gdtoa-%.o: debug/gdtoa-%.c debug/arith.h
	$(CC) -g -DINFNAN_CHECK -c $< -o $@

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
-include ../Rules.make

endif
