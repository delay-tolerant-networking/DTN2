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

echo ""
echo "removing dtn files..."
for f in /usr/bin/dtnd /etc/dtn.conf ; do
   echo "$f..."
   rm -f $f
done

for d in /var/dtn/db /var/dtn/bundles /var/dtn ; do
   echo "$d..."
   rm -rf $d
done

echo "uninstallation complete."
