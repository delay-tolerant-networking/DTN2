#
# Makefile for DTN2/servlib
#

#
# Source and object rules
#

BUNDLING_SRCS := 				\
	bundling/Bundle.cc			\
	bundling/BundleForwarder.cc		\
	bundling/BundleList.cc			\
	bundling/BundlePayload.cc		\
	bundling/BundleProtocol.cc		\
	bundling/BundleTuple.cc			\
	bundling/Contact.cc			\
	bundling/FragmentManager.cc		\
	bundling/Interface.cc			\
	bundling/InterfaceTable.cc		\

CONV_LAYER_SRCS :=				\
	conv_layers/ConvergenceLayer.cc		\
	conv_layers/FileConvergenceLayer.cc	\
	conv_layers/IPConvergenceLayer.cc	\
	conv_layers/TCPConvergenceLayer.cc	\
	conv_layers/UDPConvergenceLayer.cc	\

CMD_SRCS :=					\
	cmd/Command.cc				\
	cmd/Options.cc				\
	cmd/BundleCommand.cc			\
	cmd/HelpCommand.cc 			\
	cmd/InterfaceCommand.cc			\
	cmd/LogCommand.cc			\
	cmd/ParamCommand.cc			\
	cmd/QuitCommand.cc 			\
	cmd/RegistrationCommand.cc		\
	cmd/RouteCommand.cc			\
	cmd/StorageCommand.cc			\
	cmd/TestCommand.cc			\

DEBUG_SRCS :=					\
	debug/gdtoa-dmisc.c			\
	debug/gdtoa-dtoa.c			\
	debug/gdtoa-gdtoa.c			\
	debug/gdtoa-gmisc.c			\
	debug/gdtoa-misc.c			\
	debug/Formatter.cc			\
	debug/Log.cc				\

IO_SRCS :=					\
	io/IO.cc				\
	io/NetUtils.cc				\
	io/FdIOClient.cc			\
	io/FileIOClient.cc			\
	io/IPSocket.cc				\
	io/IPClient.cc				\
	io/TCPClient.cc				\
	io/TCPServer.cc				\
	io/UDPClient.cc				\
	io/UDPServer.cc				\

REGISTRATION_SRCS :=				\
	reg/Registration.cc			\
	reg/RegistrationTable.cc		\
	reg/RateEstimatorRegistration.cc	\
	reg/LoggingRegistration.cc		\
	reg/TclRegistration.cc			\

ROUTING_SRCS :=					\
	routing/RouteTable.cc			\
	routing/BundleRouter.cc			\
	routing/FloodBundleRouter.cc		\

STORAGE_SRCS :=					\
	storage/Serialize.cc			\
	storage/BundleStore.cc			\
	storage/GlobalStore.cc			\
	storage/PersistentStore.cc		\
	storage/RegistrationStore.cc		\
	storage/MarshalSerialize.cc		\
	storage/StorageConfig.cc		\
	storage/BerkeleyDBStore.cc		\
	storage/BerkeleyDBBundleStore.cc	\
	storage/BerkeleyDBGlobalStore.cc	\
	storage/BerkeleyDBRegistrationStore.cc	\
	storage/SQLSerialize.cc			\
	storage/SQLStore.cc			\
	storage/SQLImplementation.cc		\
        storage/SQLBundleStore.cc		\
        storage/SQLGlobalStore.cc		\
        storage/SQLRegistrationStore.cc		\
        storage/PostgresSQLImplementation.cc    \
        storage/MysqlSQLImplementation.cc       \

THREAD_SRCS :=					\
	thread/Mutex.cc				\
	thread/Notifier.cc			\
	thread/Thread.cc			\
	thread/Timer.cc				\

UTIL_SRCS :=					\
	util/RateEstimator.cc			\
	util/StringBuffer.cc			\
	util/URL.cc				\

SERVLIB_SRCS := $(BUNDLING_SRCS) 		\
		$(CONV_LAYER_SRCS)		\
		$(CMD_SRCS) 			\
		$(DEBUG_SRCS) 			\
		$(IO_SRCS) 			\
		$(REGISTRATION_SRCS)		\
		$(ROUTING_SRCS)			\
		$(STORAGE_SRCS)			\
		$(THREAD_SRCS)			\
		$(UTIL_SRCS)			\

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

#
# And a special rule to build the tcl-command.c file from command.tcl
#
cmd/Command.cc: cmd/command-init-tcl.c
cmd/command-init-tcl.c: cmd/command-init.tcl
	rm -f $@
	echo "static const char* INIT_COMMAND = " > $@;
	cat $^ | sed 's|"|\\"|g' | sed 's|^|"|g' | sed "s|$$|\\\\n\"|g" >> $@;
	echo ";">> $@

GENFILES += debug/arith.h debug/arith-native.h debug/arithchk

#
# Include the common rules here to define symbols that may be needed
# in the special rules below
#
SRCDIR := ..
-include $(SRCDIR)/Rules.make

endif
