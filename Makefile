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

alltest:
	$(MAKE) all
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

Rules.make.in:
	@echo SRCDIR: $(SRCDIR)
	@echo error -- Makefile did not set SRCDIR properly
	@exit 1

Rules.make: Rules.make.in configure
	@[ ! -z `echo "$(MAKECMDGOALS)" | grep clean` ] || \
	(echo "$@ is out of date, need to rerun configure" && \
	exit 1)

-include Rules.make
