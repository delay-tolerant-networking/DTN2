#
# Top level makefile for DTN2.
#

#
# Define a set of dispatch rules that simply call make on the
# constituent subdirectories. Note that the test directory isn't built
# by default.
#

all:   applib servlib daemon apps sim
alltest:
	$(MAKE) all
	$(MAKE) test

check:    applib servlib daemon apps test sim
clean:    applib servlib daemon apps test sim
objclean: applib servlib daemon apps test sim
depclean: applib servlib daemon apps test sim
genclean: applib servlib daemon apps test sim
binclean: applib servlib daemon apps test sim

.PHONY: applib servlib daemon apps test sim
applib servlib daemon apps test sim:
	$(MAKE) -C $@ $(MAKECMDGOALS)

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
