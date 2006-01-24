#
# Top level makefile for DTN2.
#

#
# Define a set of dispatch rules that simply call make on the
# constituent subdirectories. Note that the test directory isn't built
# by default.
#

SUBDIRS := oasys applib servlib daemon apps

all: checkconfigure $(SUBDIRS)

#
# If srcdir/builddir aren't set by some other makefile, 
# then default to .
#
ifeq ($(SRCDIR),)
SRCDIR	 := .
BUILDDIR := .
endif

#
# Include the common rules
#
-include Rules.make

#
# Dependency rules between subdirectories needed for make -j
#
applib servlib: oasys dtn-version.o
daemon: applib servlib
apps: applib
sim: servlib

#
# Rules for the version files
#
dtn-version.o: dtn-version.c
dtn-version.c: dtn-version.h
dtn-version.h: dtn-version.h.in version.dat
	$(SRCDIR)/tools/subst-version < $(SRCDIR)/dtn-version.h.in > dtn-version.h

vpath dtn-version.h.in $(SRCDIR)
vpath dtn-version.h    $(SRCDIR)
vpath dtn-version.c    $(SRCDIR)
vpath version.dat      $(SRCDIR)

bump-version:
	cd $(SRCDIR) && tools/bump-version

#
# Test rules
#
tests: 
	$(MAKE) all
	$(MAKE) -C oasys tests
	$(MAKE) -C test

#
# Install/uninstall rules
#
install:
	@tools/install.sh

uninstall:
	@tools/uninstall.sh

#
# Generate the doxygen documentation
#
doxygen:
	doxygen doc/doxyfile

#
# Pull in the rules for building a debian package
#
-include debian/debpkg.mk

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

#
# And a rule to make sure that configure has been run recently enough.
#
.PHONY: checkconfigure
checkconfigure: Rules.make

Rules.make: $(SRCDIR)/configure $(SRCDIR)/oasys/Rules.make.in 
	@[ ! x`echo "$(MAKECMDGOALS)" | grep clean` = x ] || \
	(echo "$@ is out of date, need to rerun configure" && \
	exit 1)

$(SRCDIR)/configure $(SRCDIR)/oasys/Rules.make.in:
	@echo SRCDIR: $(SRCDIR)
	@echo error -- Makefile did not set SRCDIR properly
	@exit 1

GENFILES = doc/manual/man/*.txt
