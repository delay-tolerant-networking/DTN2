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
	bundling/BundleTuple.cc			\
	bundling/Contact.cc			\
	bundling/Interface.cc			\

CONV_LAYER_SRCS :=				\
	conv_layers/ConvergenceLayer.cc		\
	conv_layers/FileConvergenceLayer.cc	\
	conv_layers/IPConvergenceLayer.cc	\
	conv_layers/TCPConvergenceLayer.cc	\
	conv_layers/UDPConvergenceLayer.cc	\

CMD_SRCS :=					\
	cmd/Command.cc				\
	cmd/Options.cc				\
						\
	cmd/BundleCommand.cc			\
	cmd/InterfaceCommand.cc			\
	cmd/LogCommand.cc			\
	cmd/StorageCommand.cc			\

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

ROUTING_SRCS :=					\
	routing/RouteTable.cc			\

STORAGE_SRCS :=					\
	storage/BerkeleyDBStore.cc		\
	storage/BundleStore.cc			\
	storage/MarshalSerialize.cc		\
	storage/PersistentStore.cc		\
	storage/RegistrationStore.cc		\
	storage/SQLSerialize.cc			\
	storage/SQLStore.cc			\
	storage/Serialize.cc			\
	storage/StorageSystem.cc		\
#	storage/PostgresSQLExtract.cc		\
#	storage/PostgresSQLStore.cc		\

THREAD_SRCS :=					\
	thread/Mutex.cc				\
	thread/Thread.cc			\

UTIL_SRCS :=					\
	util/StringBuffer.cc			\

SERVLIB_SRCS := $(BUNDLING_SRCS) 		\
		$(CONV_LAYER_SRCS)		\
		$(CMD_SRCS) 			\
		$(DEBUG_SRCS) 			\
		$(IO_SRCS) 			\
		$(ROUTING_SRCS)			\
		$(STORAGE_SRCS)			\
		$(THREAD_SRCS)			\
		$(UTIL_SRCS)			\

SERVLIB_OBJS := $(SERVLIB_SRCS:.cc=.o)
SERVLIB_OBJS := $(SERVLIB_OBJS:.c=.o)

ALLSRCS := $(SERVLIB_SRCS)

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
