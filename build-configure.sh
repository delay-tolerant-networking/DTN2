#!/bin/sh
#
# Simple script to run various autoconf related commands to generate
# the configure script. 
#
# This should not need to be run often, only when things like library
# dependencies change. In particular, all output files are cross-platform
# and are therefore checked into CVS.

trap 'rm -f aclocal.m4 ; exit 0' 0 1 2 3 13 15

echo "preconfig: building aclocal.m4..."
rm -f aclocal.m4
cat aclocal/*.ac > aclocal.m4

if [ -d oasys ] ; then
    echo "preconfig: loading oasys functions into aclocal.m4..."
    cat oasys/aclocal/*.ac >> aclocal.m4
else
    echo "preconfig: ERROR -- can't find oasys for autoconf macros"
    exit 1
fi

echo "preconfig: running autoscan to find missing checks..."
autoscan

echo "preconfig: running autoheader to build config.h.in..."
rm -f auto.config.h auto.config.h.in
autoheader
chmod 444 auto.config.h.in

echo "preconfig: running autoconf to build configure..."
rm -f configure
autoconf
chmod 555 configure

echo "preconfig: purging configure cache..."
rm -rf autom4te.cache
rm -f config.cache

echo "preconfig: done."
