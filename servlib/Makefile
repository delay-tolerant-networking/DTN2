#
# Makefile for DTN2/servlib
#

#
# Source and object rules
#

BUNDLING_SRCS := 				\
	bundling/Bundle.cc			\
	bundling/BundleList.cc			\
	bundling/BundlePayload.cc		\
	bundling/BundleProtocol.cc		\
	bundling/BundleTuple.cc			\
	bundling/Contact.cc			\
	bundling/Interface.cc			\
	bundling/InterfaceTable.cc		\
	bundling/BundleForwarding.cc		\

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
	cmd/InterfaceCommand.cc			\
	cmd/LogCommand.cc			\
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
	io/IPSocket.cc				\
	io/TCPClient.cc				\
	io/TCPServer.cc				\

REGISTRATION_SRCS :=				\
	reg/Registration.cc			\
	reg/RegistrationTable.cc		\

ROUTING_SRCS :=					\
	routing/RouteTable.cc			\

STORAGE_SRCS :=					\
	storage/Serialize.cc			\
	storage/BundleStore.cc			\
	storage/GlobalStore.cc			\
	storage/PersistentStore.cc		\
	storage/RegistrationStore.cc		\
	storage/MarshalSerialize.cc		\
	storage/BerkeleyDBStore.cc		\
	storage/SQLSerialize.cc			\
	storage/SQLStore.cc			\
	storage/SQLImplementation.cc		\
        storage/SQLBundleStore.cc		\
        storage/SQLGlobalStore.cc		\
        storage/SQLRegistrationStore.cc		\
	storage/PostgresSQLImplementation.cc	\
	storage/MysqlSQLImplementation.cc	\

THREAD_SRCS :=					\
	thread/Mutex.cc				\
	thread/Notifier.cc			\
	thread/Thread.cc			\

UTIL_SRCS :=					\
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
debug/gdtoa-%.o: debug/gdtoa-%.c debug/arith.h
	$(CC) -g -DINFNAN_CHECK -c $< -o $@

debug/arith.h: debug/gdtoa-arithchk.c
	$(CC) $(DEBUG) $(OPTIMIZE) $< -o debug/arithchk
	debug/arithchk > debug/arith.h
	rm -f debug/arithchk

GENFILES += debug/arith.h debug/arithchk

#
# Include the common rules
#
SRCDIR := ..
-include $(SRCDIR)/Rules.make

endif
