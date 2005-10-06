#
# Common simple makefile for DTN2 applications. Apps should define the SRCS
# and APP variables


OBJS := $(SRCS:.cc=.o)
OBJS := $(OBJS:.c=.o)

MAN := $(APP).1
MANTXT := $(TOPDIR)/doc/manual/man/$(APP).txt

BINFILES := $(APP)
all: $(APP) $(MANTXT)

APPLFLAGS = -L$(TOPDIR)/applib -L$(TOPDIR)/oasys $(LDFLAGS) \
		-ldtnapi -loasyscompat $(LIBS) 

$(APP): $(OBJS) $(TOPDIR)/applib/libdtnapi.a
	$(CXX) $(OBJS) $(APPLFLAGS) -o $@

# this next line and the dash to ignore errs are because not all the
# manpages are done. When they are, we'll remove these to cause
# an error when there is no man page.
$(MAN):
	@echo "NOTE: no manpage exists for $(APP)"

$(MANTXT): $(MAN)
	@if [ -f $(MAN) ] ; then  man ./$(MAN) | col -b > $@ ; fi

#
# Fix up the SRCDIR
#
ifeq ($(SRCDIR),)
SRCDIR := ../..
endif

#
# Include the common rules
#
-include $(TOPDIR)/Rules.make

CPPFLAGS += -I$(SRCDIR)/applib

