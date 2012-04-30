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
        bundling/AgeBlockProcessor.cc           \
        bundling/APIBlockProcessor.cc           \
	bundling/BlockInfo.cc			\
	bundling/BlockProcessor.cc		\
	bundling/Bundle.cc			\
	bundling/BPQBlockProcessor.cc			\
	bundling/BPQBlock.cc			\
	bundling/BPQCache.cc			\
	bundling/BPQCacheEntry.cc			\
	bundling/BPQFragmentList.cc			\
	bundling/BPQResponse.cc			\
	bundling/BundleActions.cc		\
	bundling/BundleDaemon.cc		\
	bundling/BundleDetail.cc		\
	bundling/BundleEventHandler.cc		\
	bundling/BundleInfoCache.cc		\
	bundling/BundleList.cc			\
	bundling/BundleMappings.cc		\
	bundling/BundlePayload.cc		\
	bundling/BundleProtocol.cc		\
	bundling/BundleStatusReport.cc		\
	bundling/BundleTimestamp.cc		\
	bundling/CustodySignal.cc		\
	bundling/CustodyTimer.cc		\
	bundling/Dictionary.cc          	\
	bundling/ForwardingInfo.cc		\
	bundling/ForwardingLog.cc		\
	bundling/FragmentManager.cc		\
	bundling/FragmentState.cc		\
	bundling/ExpirationTimer.cc		\
	bundling/GbofId.cc  		        \
	bundling/MetadataBlock.cc		\
	bundling/MetadataBlockProcessor.cc	\
	bundling/PayloadBlockProcessor.cc	\
	bundling/PreviousHopBlockProcessor.cc	\
	bundling/PrimaryBlockProcessor.cc	\
	bundling/SDNV.cc			\
	bundling/SessionBlockProcessor.cc	\
	bundling/SequenceID.cc			\
	bundling/SequenceIDBlockProcessor.cc	\
	bundling/UnknownBlockProcessor.cc	\
	bundling/S10Logger.cc	\

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
	conv_layers/AX25CMConvergenceLayer.cc \
	conv_layers/BluetoothConvergenceLayer.cc  \
	conv_layers/ConnectionConvergenceLayer.cc \
	conv_layers/CLConnection.cc 		\
	conv_layers/ConvergenceLayer.cc		\
	conv_layers/EthConvergenceLayer.cc	\
	conv_layers/FileConvergenceLayer.cc	\
	conv_layers/IPConvergenceLayer.cc	\
	conv_layers/IPConvergenceLayerUtils.cc	\
	conv_layers/NullConvergenceLayer.cc	\
	conv_layers/SeqpacketConvergenceLayer.cc \
	conv_layers/SerialConvergenceLayer.cc   \
	conv_layers/StreamConvergenceLayer.cc   \
	conv_layers/TCPConvergenceLayer.cc	\
	conv_layers/UDPConvergenceLayer.cc	\
	conv_layers/LTPConvergenceLayer.cc	\
	conv_layers/NORMConvergenceLayer.cc	\
	conv_layers/NORMReceiver.cc		\
	conv_layers/NORMSender.cc		\
	conv_layers/NORMSessionManager.cc	\
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
	cmd/BlockCommand.cc			\
	cmd/BundleCommand.cc			\
	cmd/BPQCommand.cc			\
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
	cmd/SecurityCommand.cc			\

NAMING_SRCS :=					\
	naming/EndpointID.cc			\
	naming/EndpointIDOpt.cc			\
	naming/Scheme.cc			\
	naming/SchemeTable.cc			\
	naming/SessionScheme.cc			\
	naming/DTNScheme.cc			\
	naming/EthernetScheme.cc		\
        naming/IPNScheme.cc                     \
	naming/SessionScheme.cc			\
	naming/StringScheme.cc			\
	naming/WildcardScheme.cc		\
	naming/TCAScheme.cc			\

PROPHET_SRCS :=					\
	prophet/AckList.cc			\
	prophet/BundleOffer.cc			\
	prophet/BundleTLV.cc			\
	prophet/Controller.cc			\
	prophet/Decider.cc			\
	prophet/Dictionary.cc			\
	prophet/Encounter.cc			\
	prophet/HelloTLV.cc			\
	prophet/Node.cc				\
	prophet/ProphetTLV.cc			\
	prophet/Repository.cc			\
	prophet/RIBDTLV.cc			\
	prophet/RIBTLV.cc			\
	prophet/Stats.cc			\
	prophet/Table.cc			\
	prophet/OfferTLV.cc			\
	prophet/ResponseTLV.cc			\

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
	routing/DTLSR.cc			\
	routing/DTLSRConfig.cc			\
	routing/DTLSRRouter.cc			\
	routing/ExternalRouter.cc		\
	routing/BundleRouter.cc			\
	routing/FloodBundleRouter.cc		\
	routing/LinkScheduleEstimator.cc	\
	routing/ProphetBundleCore.cc		\
	routing/ProphetBundleList.cc		\
	routing/ProphetLinkList.cc		\
	routing/ProphetNode.cc			\
	routing/ProphetNodeList.cc		\
	routing/ProphetRouter.cc		\
	routing/RouteEntry.cc			\
	routing/RouteTable.cc			\
	routing/RouterInfo.cc			\
	routing/TableBasedRouter.cc		\
	routing/TcaEndpointID.cc                \
	routing/TcaControlBundle.cc             \
	routing/TcaRouter.cc                    \

SESSION_SRCS :=					\
	session/Session.cc			\
	session/SessionTable.cc			\
	session/Subscriber.cc			\

OLD_STORAGE_SRCS :=				\
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

SECURITY_SRCS :=				\
	security/Ciphersuite.cc			\
	security/BA_BlockProcessor.cc		\
	security/PI_BlockProcessor.cc		\
	security/PC_BlockProcessor.cc		\
	security/ES_BlockProcessor.cc		\
	security/Ciphersuite_BA1.cc		\
	security/Ciphersuite_PI2.cc		\
	security/Ciphersuite_PC3.cc		\
	security/Ciphersuite_ES4.cc		\
	security/BP_Tag.cc			\
	security/KeyDB.cc			\
	security/SPD.cc				\
	security/KeySteward.cc			\
	gcm/gcm_aes.c				\
	gcm/gcm.cc				\
	gcm/gf128mul.cc				\

SERVLIB_SRCS := 				\
	DTNServer.cc				\
	$(BUNDLING_SRCS) 			\
	$(CONTACT_SRCS)				\
	$(CONV_LAYER_SRCS)			\
	$(DISCOVERY_SRCS)			\
	$(CMD_SRCS) 				\
	$(NAMING_SRCS) 				\
	$(PROPHET_SRCS) 			\
	$(REGISTRATION_SRCS)			\
	$(ROUTING_SRCS)				\
	$(SESSION_SRCS)				\
	$(STORAGE_SRCS)				\
	$(SECURITY_SRCS)			\

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
