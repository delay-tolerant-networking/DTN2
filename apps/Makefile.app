#
# Common simple makefile for DTN2 applications. Apps should define the SRCS
# and APP variables


OBJS := $(SRCS:.cc=.o)
OBJS := $(OBJS:.c=.o)

all: $(APP)

$(APP): $(OBJS) $(SRCDIR)/applib/libdtnapi.a
	$(CXX) $(OBJS) -L$(SRCDIR)/applib -L$(SRCDIR)/oasys $(LDFLAGS) \
		-loasyscompat -ldtnapi $(LIBS) -o $@

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make

CPPFLAGS += -I $(SRCDIR)/applib
