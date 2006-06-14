#
# Makefile for DTN2/apps
#

ifeq ($(SRCDIR),)
SRCDIR := ..
endif

SINGLE_SOURCE_APPS := 			\
	dtncat/dtncat			\
	dtncp/dtncp			\
	dtncpd/dtncpd			\
	dtnping/dtnping			\
	dtnperf/dtnperf-client		\
	dtnperf/dtnperf-server		\
	dtnrecv/dtnrecv			\
	dtnsend/dtnsend			\

OTHER_APPS :=				\
	dtnmoteproxy/dtnmoteproxy	\
	dtntest/dtntest			\
	dtntunnel/dtntunnel		\
	tca_admin/tca_admin		\

APPS := $(SINGLE_SOURCE_APPS) $(OTHER_APPS)

all: ../applib/libdtnapi.a $(APPS) $(MANPAGES)

# Fake build target in case the api lib doesn't exist, but allows us
# to include it as a dependency above so the apps get properly rebuilt
# for the 'all' target
../applib/libdtnapi.a:
	@echo "ERROR: libdtnapi.a must be built before the apps can"
	exit 1

include ../Rules.make
CFLAGS += -I$(SRCDIR)/applib
LDFLAGS += -L../applib -L../oasys
DTN_LIBS := -ldtnapi -loasyscompat

BINFILES += $(APPS)

#
# For dtnmoteproxy, don't worry about cast alignment
#
dtnmoteproxy/dtnmoteproxy.o: dtnmoteproxy/dtnmoteproxy.c
	@rm -f $@; mkdir -p $(@D)
	$(CC) $(CFLAGS) -Wno-cast-align -c $< -o $@

dtnmoteproxy/serialsource.o: dtnmoteproxy/serialsource.c
	@rm -f $@; mkdir -p $(@D)
	$(CC) $(CFLAGS) -Wno-cast-align -c $< -o $@

dtnmoteproxy/dtnmoteproxy: dtnmoteproxy/dtnmoteproxy.o \
			   dtnmoteproxy/serialsource.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(DTN_LIBS) $(LIBS)

#
# Rules for dtntest
#
dtntest/dtntest: dtntest/dtntest.o
	$(CXX) $(CFLAGS) $^ -o $@ \
		$(LDFLAGS) $(DTN_LIBS) -loasys $(OASYS_LIBS) $(LIBS) 

#
# Rules for dtntunnel
#
dtntunnel/dtntunnel: dtntunnel/DTNTunnel.o \
		     dtntunnel/TCPTunnel.o \
		     dtntunnel/UDPTunnel.o
	$(CXX) $(CFLAGS) $^ -o $@ \
		$(LDFLAGS) $(DTN_LIBS) -loasys $(OASYS_LIBS) $(LIBS) 

#
# Rules for tca_admin
#
tca_admin/tca_admin: tca_admin/tca_admin.o \
                tca_admin/TcaEndpointID.o \
                ../servlib/routing/TcaControlBundle.o \
                tca_admin/TcaRegistry.o \
                tca_admin/TcaController.o \
                tca_admin/libs/gateway_prot_clnt.o \
                tca_admin/libs/gateway_prot_xdr.o \
                tca_admin/libs/gateway_rpc.o \
                tca_admin/libs/sha1.o
	$(CXX) $(CFLAGS) $^ -o \
		$@ $(LDFLAGS) $(DTN_LIBS) -loasys $(OASYS_LIBS) $(LIBS)

#
# Default rule for all single source apps
#
$(SINGLE_SOURCE_APPS): %: %.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(DTN_LIBS) $(LIBS)

