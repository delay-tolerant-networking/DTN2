#
#    Copyright 2004-2006 Intel Corporation
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

#
# Makefile for DTN2/apps
#

ifeq ($(SRCDIR),)
BUILDDIR := ..
SRCDIR := ..
endif

SINGLE_SOURCE_APPS := 			\
	dtncat/dtncat			\
	dtncp/dtncp			\
	dtncpd/dtncpd			\
	dtnping/dtnping			\
	dtnping/dtntraceroute		\
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

DTN_LIBS := ../applib/libdtnapi.a ../oasys/liboasyscompat.a
DTN_LIBS++ := $(DTN_LIBS) ../applib/libdtnapi++.a ../oasys/liboasys.a

all: $(DTN_LIBS++) $(APPS) $(MANPAGES)

# Fake build target in case the api lib doesn't exist, but allows us
# to include it as a dependency above so the apps get properly rebuilt
# for the 'all' target
$(DTN_LIBS++):
	@echo "ERROR: $@ must be built before the apps can"
	exit 1

include ../Rules.make
CPPFLAGS += -I$(SRCDIR)/applib
LDFLAGS += -L../applib -L../oasys

BINFILES += $(APPS)

#
# For dtnmoteproxy, don't worry about cast alignment
#
dtnmoteproxy/dtnmoteproxy.o: dtnmoteproxy/dtnmoteproxy.c
	@rm -f $@; mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -Wno-cast-align $(CFLAGS) -c $< -o $@

dtnmoteproxy/serialsource.o: dtnmoteproxy/serialsource.c
	@rm -f $@; mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -Wno-cast-align $(CFLAGS) -c $< -o $@

dtnmoteproxy/dtnmoteproxy: dtnmoteproxy/dtnmoteproxy.o \
			   dtnmoteproxy/serialsource.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(DTN_LIBS) $(LIBS)

#
# Rules for dtntest
#
dtntest/dtntest: dtntest/dtntest.o
	$(CXX) $(CXXFLAGS) $^ -o $@ \
		$(LDFLAGS) $(DTN_LIBS++) $(OASYS_LIBS) $(LIBS) 

#
# Rules for dtntunnel
#
dtntunnel/dtntunnel: dtntunnel/DTNTunnel.o \
		     dtntunnel/TCPTunnel.o \
		     dtntunnel/UDPTunnel.o
	$(CXX) $(CXXFLAGS) $^ -o $@ \
		$(LDFLAGS) $(DTN_LIBS++) $(OASYS_LIBS) $(LIBS) 

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
	$(CXX) $(CXXFLAGS) $^ -o \
		$@ $(LDFLAGS) $(DTN_LIBS++) $(OASYS_LIBS) $(LIBS)

#
# Default rule for all single source apps
#
$(SINGLE_SOURCE_APPS): %: %.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(DTN_LIBS) $(LIBS)

