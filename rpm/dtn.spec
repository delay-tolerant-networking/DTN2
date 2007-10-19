Summary: Delay Tolerant Networking reference implementation
Name: dtn
Version: 2.5.0
Release: 1
License: Apache
Group: Applications/Internet
URL: http://www.dtnrg.org
Source0: %{name}_%{version}.tgz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description

 This package contains the reference implementation of the Delay
 Tolerant Networking (DTN) architecture. This includes the DTN daemon
 (dtnd) as well as several example applications that use the DTN
 protocols. 

%prep
%setup -q -n %{name}_%{version}

%build
./configure
make

%install
rm -rf $RPM_BUILD_ROOT
DESTDIR=$RPM_BUILD_ROOT make install

%clean
make clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%doc README
%config /etc/dtn.conf
%config /etc/DS.xsd
%config /etc/clevent.xsd
%config/etc/router.xsd

/var/dtn
/var/dtn/bundles
/var/dtn/db
/usr/bin/dtnd
/usr/bin/dtnd-control
/usr/bin/dtncat
/usr/bin/dtncp
/usr/bin/dtncpd
/usr/bin/dtnhttpproxy
/usr/bin/dtnping
/usr/bin/dtnrecv
/usr/bin/dtnsend
/usr/bin/dtntunnel
/usr/bin/dtntraceroute
/usr/lib/libdtnapi++-2.5.0.so
/usr/lib/libdtnapi++.so
/usr/lib/libdtnapi-2.5.0.so
/usr/lib/libdtnapi.so
/usr/lib/libdtntcl-2.5.0.so
/usr/lib/libdtntcl.so
/usr/lib/liboasys.so
/usr/lib/liboasyscompat.so


%changelog

