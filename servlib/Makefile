#
# Makefile for DTN2/servlib
#

#
# Source and object rules
#
SERVLIB_OBJS :=

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


