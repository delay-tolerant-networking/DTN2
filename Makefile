#
# Top level makefile for DTN2.
#

# Default rule is to build the server library, the daemon, and all
# applications
all: servlib daemon apps

# Build the server library
.PHONY: servlib
servlib: 
	$(MAKE) -C servlib $(MAKECMDGOALS)

# Build the daemon
.PHONY: daemon
daemon: 
	$(MAKE) -C daemon $(MAKECMDGOALS)

.PHONY: apps
apps: 
	$(MAKE) -C apps $(MAKECMDGOALS)


# Generate the doxygen documentation
doxygen:
	doxygen doc/doxyfile

#
# Build a TAGS database. Note this includes all the sources so it gets
# header files as well.
#
.PHONY: TAGS tags
tags TAGS:
	cd $(SRCDIR) && \
	find . -name \*.h -or -name \*.c -or -name \*.cc | \
		xargs etags -l c++
	find . -name \*.h -or -name \*.c -or -name \*.cc | \
		xargs ctags 

SRCDIR := .
-include $(SRCDIR)/Rules.make
