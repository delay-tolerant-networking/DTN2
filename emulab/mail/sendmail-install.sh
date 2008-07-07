#!/bin/sh

cluster=$1
num=$2
mode=$3

if [ -z "$cluster" -o -z "$num" -o -z "$mode" ] ; then
    echo "usage: $0 cluster num mode"
    exit 1
fi

i=0

last=$(($num-1))
last2=$(($num-2))

while [ $i != $num ] ; do

j=$(($i + 1))
echo "Installing sendmail config on node $i..."

if [ ! $i == $last ] ; then
if [ $mode == "hop" ] ; then
    smarthost=node-$j-links-$i-$j
else
    smarthost=node-$last-links-$last2-$last
fi
else
    smarthost=
fi

sed "s/__SMART_HOST__/$smarthost/" < sendmail.cf.template > /tmp/sendmail.cf.$i

node=node-$i.$cluster.dtn.emulab.net

ssh $node sudo /etc/init.d/sendmail stop

scp /tmp/sendmail.cf.$i $node:/tmp/sendmail.cf
ssh $node sudo cp /tmp/sendmail.cf /etc/mail/sendmail.cf

ssh $node sudo find /var/spool/mqueue/ -type f -delete   
ssh $node sudo find /var/spool/clientmqueue/ -type f -delete   
ssh $node rm -f /var/log/maillog

ssh $node sudo rm -rf /var/run/sendmail_host_status
ssh $node sudo mkdir /var/run/sendmail_host_status
ssh $node sudo chmod 700 /var/run/sendmail_host_status

scp count_delivery.sh $node:/tmp/count_delivery.sh

ssh $node sh -c 'cat > /tmp/sysconfig_sendmail <<EOF
DAEMON=yes
QUEUE=5s
EOF
'
ssh $node sudo cp /tmp/sysconfig_sendmail /etc/sysconfig/sendmail

ssh $node sudo /etc/init.d/sendmail start

i=$(($i + 1))
done
