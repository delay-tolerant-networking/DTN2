#
# Makefile for DTN2/servlib
#

#
# Source and object rules
#

BUNDLING_SRCS := 				\
	bundling/AddressFamily.cc		\
	bundling/Bundle.cc			\
	bundling/BundleActions.cc		\
	bundling/BundleConsumer.cc		\
	bundling/BundleDaemon.cc		\
	bundling/BundleList.cc			\
	bundling/BundlePayload.cc		\
	bundling/BundleProtocol.cc		\
	bundling/BundleTuple.cc			\
	bundling/BundleStatusReport.cc		\
	bundling/Contact.cc			\
	bundling/ContactManager.cc		\
	bundling/FixedAddressFamily.cc		\
	bundling/FragmentManager.cc		\
	bundling/Interface.cc			\
	bundling/InterfaceTable.cc		\
	bundling/InternetAddressFamily.cc	\
	bundling/EthernetAddressFamily.cc	\
	bundling/Link.cc			\
	bundling/OndemandLink.cc		\
	bundling/OpportunisticLink.cc		\
	bundling/Peer.cc			\
	bundling/ScheduledLink.cc		\
	bundling/SmtpAddressFamily.cc		\
	bundling/StringAddressFamily.cc		\
	bundling/WildcardAddressFamily.cc	\
	bundling/tstotv.c			\
	bundling/tvtots.c			\

CONV_LAYER_SRCS :=				\
	conv_layers/ConvergenceLayer.cc		\
	conv_layers/FileConvergenceLayer.cc	\
	conv_layers/IPConvergenceLayer.cc	\
	conv_layers/EthConvergenceLayer.cc	\
	conv_layers/TCPConvergenceLayer.cc	\
	conv_layers/UDPConvergenceLayer.cc	\

CMD_SRCS :=					\
	cmd/APICommand.cc			\
	cmd/BundleCommand.cc			\
	cmd/InterfaceCommand.cc			\
	cmd/LinkCommand.cc          		\
	cmd/ParamCommand.cc			\
	cmd/RegistrationCommand.cc		\
	cmd/RouteCommand.cc			\
	cmd/StorageCommand.cc			\
	cmd/TestCommand.cc			\

REGISTRATION_SRCS :=				\
	reg/AdminRegistration.cc		\
	reg/Registration.cc			\
	reg/RegistrationTable.cc		\
	reg/LoggingRegistration.cc		\
	reg/TclRegistration.cc			\

ROUTING_SRCS :=					\
	routing/RouteTable.cc			\
	routing/BundleRouter.cc			\
	routing/FloodBundleRouter.cc		\
	routing/NeighborhoodRouter.cc		\
	routing/LinkStateGraph.cc		\

STORAGE_SRCS :=					\
	storage/BundleStore.cc			\
	storage/GlobalStore.cc			\
	storage/PersistentStore.cc		\
	storage/RegistrationStore.cc		\
	storage/StorageConfig.cc		\
	storage/BerkeleyDBStore.cc		\
	storage/SQLStore.cc			\
        storage/PostgresSQLImplementation.cc    \
        storage/MysqlSQLImplementation.cc       \

SERVLIB_SRCS := 				\
	../version.c				\
	DTNServer.cc				\
	$(BUNDLING_SRCS) 			\
	$(CONV_LAYER_SRCS)			\
	$(CMD_SRCS) 				\
	$(REGISTRATION_SRCS)			\
	$(ROUTING_SRCS)				\
	$(STORAGE_SRCS)				\

SERVLIB_OBJS := $(SERVLIB_SRCS:.cc=.o)
SERVLIB_OBJS := $(SERVLIB_OBJS:.c=.o)

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
all: servlib

servlib: libdtnserv.a
libdtnserv.a: $(SERVLIB_OBJS)
	rm -f $@
	ar ruc $@ $^
	ranlib $@ || true

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
# Include the common rules here to define symbols that may be needed
# in the special rules below
#
SRCDIR := ..
-include $(SRCDIR)/Rules.make

endif
