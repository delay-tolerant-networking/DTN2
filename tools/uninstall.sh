#!/bin/sh

#
# Simple un installation script for the dtn reference implementation
#

echo "***"
echo "*** Unstalling dtn..."
echo "***"
echo ""

echo "This script will remove all dtn executables, configuration"
echo "files and data files."
echo ""
echo -n "Are you sure you want to continue? [n]: "
read y
if [ ! "$y" = "y" -o "$y" = "yes" ]; then
   echo "uninstall aborted"
   exit 0
fi

echo -n "removing dtn executables: "
apps="dtnsend dtnrecv dtnping dtncp dtncpd"
for f in dtnd $apps ; do
   echo -n "$f... "
   rm -f /usr/bin/$f
done
echo ""

echo -n "removing dtn directories: "
for d in /var/dtn/db /var/dtn/bundles /var/dtn ; do
   echo -n "$d... "
   rm -rf $d
done
echo ""

echo "uninstallation complete."
