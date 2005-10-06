#
# Makefile for DTN2/apps
#

ifeq ($(SRCDIR),)
SRCDIR := ..
endif

SINGLE_SOURCE_APPS := 			\
	dtncp/dtncp			\
	dtncpd/dtncpd			\
	dtnping/dtnping			\
	dtnperf/dtnperf-client		\
	dtnperf/dtnperf-server		\
	dtnrecv/dtnrecv			\
	dtnsend/dtnsend			\
	dtntunnel/dtntunnel		\

OTHER_APPS :=				\
	dtnmoteproxy/dtnmoteproxy	\

APPS := $(SINGLE_SOURCE_APPS) $(OTHER_APPS)

all: $(APPS) $(MANPAGES)

-include ../Rules.make
CFLAGS  += -I$(SRCDIR)/applib
LDFLAGS += -L../applib -L../oasys
LIBS    += -ldtnapi -loasyscompat

BINFILES += $(APPS)

dtnmoteproxy/dtnmoteproxy: dtnmoteproxy/dtnmoteproxy.o dtnmoteproxy/serialsource.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS) 

$(SINGLE_SOURCE_APPS): %: %.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS) 

