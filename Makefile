#
# Top level makefile for DTN2.
#

#
# Define a set of dispatch rules that simply call make on the
# constituent subdirectories
#
all:   servlib daemon apps
clean: servlib daemon apps
check: servlib daemon apps

.PHONY: servlib daemon apps
servlib daemon apps:  
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
