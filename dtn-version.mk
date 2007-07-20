#
# Makefile to set version bindings
#

EXTRACT_VERSION := $(SRCDIR)/oasys/tools/extract-version

DTN_VERSION	  := $(shell $(EXTRACT_VERSION) $(SRCDIR)/dtn-version.dat version)
DTN_VERSION_MAJOR := $(shell $(EXTRACT_VERSION) $(SRCDIR)/dtn-version.dat major)
DTN_VERSION_MINOR := $(shell $(EXTRACT_VERSION) $(SRCDIR)/dtn-version.dat minor)
DTN_VERSION_PATCH := $(shell $(EXTRACT_VERSION) $(SRCDIR)/dtn-version.dat patch)

OASYS_VERSION	    := $(shell $(EXTRACT_VERSION) $(SRCDIR)/oasys/oasys-version.dat version)
OASYS_VERSION_MAJOR := $(shell $(EXTRACT_VERSION) $(SRCDIR)/oasys/oasys-version.dat major)
OASYS_VERSION_MINOR := $(shell $(EXTRACT_VERSION) $(SRCDIR)/oasys/oasys-version.dat minor)
OASYS_VERSION_PATCH := $(shell $(EXTRACT_VERSION) $(SRCDIR)/oasys/oasys-version.dat patch)
