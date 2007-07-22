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
# Top level makefile for DTN2.
#

#
# Define a set of dispatch rules that simply call make on the
# constituent subdirectories. Note that the test directory isn't built
# by default.
#

SUBDIRS := oasys applib servlib daemon apps sim

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
include Rules.make
include $(SRCDIR)/dtn-version.mk

#
# Dependency rules between subdirectories needed for make -j
#
applib servlib: oasys dtn-version.o
daemon: applib servlib
apps: applib servlib
sim: servlib

#
# Rules for the version files
#
dtn-version.o: dtn-version.c
dtn-version.c: dtn-version.h
dtn-version.h: dtn-version.h.in dtn-version.dat
	$(SRCDIR)/oasys/tools/subst-version $(SRCDIR)/dtn-version.dat \
		< $(SRCDIR)/dtn-version.h.in > dtn-version.h

vpath dtn-version.h.in $(SRCDIR)
vpath dtn-version.h    $(SRCDIR)
vpath dtn-version.c    $(SRCDIR)
vpath dtn-version.dat  $(SRCDIR)

bump-version:
	cd $(SRCDIR) && tools/bump-version dtn-version.dat

#
# Test rules
#
test: tests
tests: 
	$(MAKE) all
	$(MAKE) -C oasys tests
	$(MAKE) -C test

dtn-tests:
	$(MAKE) -C test

#
# Installation rules
#
install:
	for dir in $(DESTDIR)$(localstatedir)/dtn \
		   $(DESTDIR)$(localstatedir)/dtn/bundles \
		   $(DESTDIR)$(localstatedir)/dtn/db ; do \
	    (mkdir -p $$dir; chmod 755 $$dir; \
		[ x$(DTN_USER) = x ] || chown $(DTN_USER) $$dir); \
	done

	[ -d $(DESTDIR)$(bindir) ] || \
	    (mkdir -p $(DESTDIR)$(bindir); chmod 755 $(DESTDIR)$(bindir))

	[ -d $(DESTDIR)$(libdir) ] || \
	    (mkdir -p $(DESTDIR)$(libdir); chmod 755 $(DESTDIR)$(libdir))

	for prog in daemon/dtnd \
		    tools/dtnd-control \
		    apps/dtncat/dtncat \
		    apps/dtncp/dtncp \
		    apps/dtncpd/dtncpd \
		    apps/dtnping/dtnping \
		    apps/dtnping/dtntraceroute \
		    apps/dtnrecv/dtnrecv \
		    apps/dtnsend/dtnsend \
		    apps/dtntunnel/dtntunnel ; do \
	    ($(INSTALL_PROGRAM) $$prog $(DESTDIR)$(bindir)) ; \
	done

	[ x$(SHLIBS) = x ] || \
	for lib in oasys/liboasys.$(SHLIB_EXT) \
		   oasys/liboasyscompat.$(SHLIB_EXT) \
		   applib/libdtnapi-$(DTN_VERSION).$(SHLIB_EXT) \
	           applib/libdtnapi++-$(DTN_VERSION).$(SHLIB_EXT) \
	           applib/libdtntcl-$(DTN_VERSION).$(SHLIB_EXT) ; do \
	    ($(INSTALL_PROGRAM) $$lib $(DESTDIR)$(libdir)) ; \
	done

	for lib in libdtnapi libdtnapi++ libdtntcl ; do \
		(cd $(DESTDIR)$(libdir) && rm -f $$lib.$(SHLIB_EXT) && \
		 ln -s $$lib-$(DTN_VERSION).$(SHLIB_EXT) $$lib.$(SHLIB_EXT)) \
	done

	[ x$(DTN_USER) = x ] || chown -R $(DTN_USER) $(DESTDIR)$(bindir)

	[ -d $(DESTDIR)$(sysconfdir) ] || mkdir -p $(DESTDIR)$(sysconfdir)
	if [ -f $(DESTDIR)$(sysconfdir)/dtn.conf ]; then \
		echo "WARNING: $(DESTDIR)$(sysconfdir)/dtn.conf exists -- not overwriting"; \
	else \
		$(INSTALL_DATA) daemon/dtn.conf $(DESTDIR)$(sysconfdir)/dtn.conf; \
	fi

	$(INSTALL_DATA) servlib/routing/router.xsd $(DESTDIR)$(sysconfdir)/router.xsd
	$(INSTALL_DATA) servlib/conv_layers/clevent.xsd $(DESTDIR)$(sysconfdir)/clevent.xsd
	$(INSTALL_DATA) oasys/storage/DS.xsd $(DESTDIR)$(sysconfdir)/DS.xsd

#
# Generate the doxygen documentation
#
doxygen:
	doxygen doc/doxyfile

#
# Generate XML schema documentation
# Requires:
#     xsddoc-1.0 (http://xframe.sourceforge.net/xsddoc/)
#     java
#
xsddoc:
	mkdir -p doc/router-xsddoc
	xsddoc -t "External Router Interface" -o doc/router-xsddoc -q \
		servlib/routing/router.xsd
	mkdir -p doc/cla-xsddoc
	xsddoc -t "External CLA Interface" -o doc/cla-xsddoc -q \
		servlib/conv_layers/clevent.xsd
	mkdir -p doc/router-xsddoc
	xsddoc -t "External Data Store Interface" -o doc/ds-xsddoc -q \
		oasys/storage/DS.xsd

xsdbindings:
	$(MAKE) -C servlib xsdbindings
	$(MAKE) -C oasys xsdbindings

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
