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
	tca_admin/tca_admin	\

APPS := $(SINGLE_SOURCE_APPS) $(OTHER_APPS)

all: $(APPS) $(MANPAGES)

include ../Rules.make
CFLAGS  += -I$(SRCDIR)/applib
LDFLAGS += -L../applib -L../oasys
LIBS    := -ldtnapi -loasyscompat

BINFILES += $(APPS)

dtnmoteproxy/dtnmoteproxy: dtnmoteproxy/dtnmoteproxy.o \
			   dtnmoteproxy/serialsource.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS) 

tca_admin/tca_admin: tca_admin/tca_admin.o \
                tca_admin/TcaEndpointID.o \
                ../servlib/routing/TcaControlBundle.o \
                tca_admin/TcaRegistry.o \
                tca_admin/TcaController.o \
                tca_admin/libs/gateway_prot_clnt.o \
                tca_admin/libs/gateway_prot_xdr.o \
                tca_admin/libs/sha1.o
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS) 



$(SINGLE_SOURCE_APPS): %: %.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS) 

