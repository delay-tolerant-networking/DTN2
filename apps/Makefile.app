#
# Common simple makefile for DTN2 applications. Apps should define the SRCS
# and APP variables

OBJS := $(SRCS:.cc=.o)
OBJS := $(OBJS:.c=.o)

all: $(APP)

$(APP): $(OBJS) $(SRCDIR)/applib/libdtnapi.a
	$(CPP) $(OBJS) -L $(SRCDIR)/applib -ldtnapi -o $@

#
# Include the common rules
#
-include $(SRCDIR)/Rules.make
