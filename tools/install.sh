#!/bin/sh

#
# Simple installation script for the dtn reference implementation
#

echo "***"
echo "*** Installing dtn..."
echo "***"
echo ""

echo -n "user account to use for the dtn daemon [dtn]: "
read acct
[ "$acct" = "" ] && acct=dtn

grep $acct /etc/passwd >/dev/null 2> /dev/null

if [ ! $? = 0 ]; then
   echo -n "create account for $acct? [y]: "
   read y
   if [ "$y" = "" -o "$y" = "y" -o "$y" = "yes" ]; then
      echo "creating account for $acct..."
      adduser $acct
   else 
      echo "can't find account for $acct... please create and re-run install"
      exit 1
   fi
fi

echo -n "install daemon setuid? [y]: "
read setuid
[ "$setuid" = "" ] && setuid=y

echo "creating dtn directories..."
for dir in /var/dtn /var/dtn/bundles /var/dtn/db ; do
    if [ -d $dir ]; then
       echo "$dir already exists... skipping"
    else 
       echo $dir
       mkdir $dir
       chown $acct $dir
       chmod 755 $dir
       if [ ! -d $dir ] ; then
           echo "error creating $dir..."
           exit 1
       fi
    fi
done   

echo "installing dtnd executable..."
install -o $acct daemon/dtnd /usr/bin/

if [ $setuid = y ]; then
   echo "setting setuid bit on dtnd..."
   chmod 4755 /usr/bin/dtnd
fi

apps="dtnsend dtnrecv dtnping dtncp dtncpd"
echo -n "installing dtn applications: "
for app in $apps ; do
   echo -n "$app... "
   install -o $acct apps/$app/$app /usr/bin/
done
echo ""

echo "copying default configuration file to /etc/dtn.conf"
install -o $acct -m 644 daemon/dtn.conf /etc/dtn.conf

echo "running dtn daemon to create initial database..."
/usr/bin/dtnd -c "" --init-db

echo "installation complete."
