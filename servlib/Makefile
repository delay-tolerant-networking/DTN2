#
# Makefile for DTN2/servlib
#

#
# Source and object rules
#
STORAGE_SRCS :=					\
	storage/BerkeleyDBStore.cc		\
	storage/BundleStore.cc			\
	storage/PersistentStore.cc		\
	storage/RegistrationStore.cc		\
	storage/Serialize.cc			\
	storage/SQLStore.cc			\
	storage/StorageSystem.cc		\

STORAGE_OBJS := $(STORAGE_SRCS:.cc=.o)

SERVLIB_OBJS := $(STORAGE_OBJS)

#
# Default target is to build the library
#
all: servlib

servlib: libdtnserv.a
libdtnserv.a: $(SERVLIB_OBJS)
	ar ruc $@ $<
	ranlib $@ || true

#
# Include the common rules
#
SRCDIR := ..
-include $(SRCDIR)/Rules.make


