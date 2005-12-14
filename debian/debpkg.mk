#
# Debian package rules
# 

.PHONY: deb
deb:
	@echo -- Working in `pwd`
	dh_testroot
	rm -rf debian/dtnd debian/dtnd.*
	tools/subst-version < debian/control.in > debian/control
	dh_testdir
	dh_install daemon/dtnd usr/bin
	dh_install apps/dtncp/dtncp usr/bin
	dh_install apps/dtncpd/dtncpd usr/bin
	dh_install apps/dtnperf/dtnperf-client apps/dtnperf/dtnperf-server usr/bin
	dh_install apps/dtnping/dtnping usr/bin
	dh_install apps/dtnrecv/dtnrecv usr/bin
	dh_install apps/dtnsend/dtnsend usr/bin
	dh_install daemon/dtn.conf etc
	dh_installdirs var/dtnd var/dtnd
	dh_fixperms
	dh_strip
	dh_installdeb
	cp debian/control debian/dtnd/DEBIAN/control
	dh_builddeb --destdir=.

apt:
	test -d ./apt-repository/pool
	reprepro -V -b ./apt-repository -C main remove     sarge dtnd \
		|| echo "no existing dtnd"
	reprepro -V -b ./apt-repository -C main includedeb sarge \
		`ls -1rt dtnd*.deb | tail -1`
