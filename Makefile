#
# Top level makefile for DTN2.
#

#
# Define a set of dispatch rules that simply call make on the
# constituent subdirectories. Note that the test directory isn't built
# by default.
#

all:   servlib daemon apps
alltest:
	$(MAKE) all
	$(MAKE) test
check: servlib daemon apps test
clean: servlib daemon apps test
objclean: servlib daemon apps test
depclean: servlib daemon apps test
genclean: servlib daemon apps test
binclean: servlib daemon apps test

.PHONY: servlib daemon apps test
servlib daemon apps test:
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
