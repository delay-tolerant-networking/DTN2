#!/bin/csh

# Now generate all the sendmail.mc files
# Now generate daemon for the node
    
set idplus  = `expr $id + 1`
set idminus = `expr $id - 1`
    
set local_ip = node-$id-link-$idminus
set next_ip  = node-$idplus-link-$id
set next_relay = node-$idplus
set start_ip = node-1
set final_ip = node-$maxnodes
set final_link = `expr $maxnodes - 1`
if ($id == 1) then
    set local_mail_domain = node-$id.emulab.net
else
    set local_mail_domain = node-$id-link-$final_link.emulab.net
endif
set dest_addr = "$username@$final_ip"
    
#create the sendmail.mc file
echo "Copying the sendmail template ... and copying to $logroot/sendmail-$id.mc .." >>& $info
sudo cp -f $dtn2testroot/mail/sendmail-template.mc $logroot/sendmail-$id.mc >>& $info
sed -i "s/__SMART_HOST__/$next_relay/g" $logroot/sendmail-$id.mc  >>& $info
sed -i "s/__LOCAL_DOMAIN__/$local_mail_domain/g" $logroot/sendmail-$id.mc  >>& $info

#install the new mc file
echo "Install the new sendmail file ..." >>& $info
sudo m4 $logroot/sendmail-$id.mc > $logroot/sendmail-$id.cf 
sudo cp -f $logroot/sendmail-$id.cf /etc/mail/sendmail.cf

#remove the default mailbox
set defaultmailbox=$logroot/default.$id
rm -f $defaultmailbox
set matchmailfile=$logroot/match.$id
rm -f $matchmailfile

set mailrc=$logroot/procmailrc.$id

rm -f $userhome/.procmailrc >>& $info
sudo rm -f /etc/procmailrc >>& $info
rm -f $ftplogfile >>& $info 
touch $ftplogfile >>& $info
touch $defaultmailbox >>& $info
touch $matchmailfile >>& $info

#install the got_file file
if($id == $maxnodes) then

    #install the .procmailrc file
    echo "Copy the new procmailrc file ..." >>& $info
    cp -f $dtn2testroot/mail/procmailrc $mailrc  >>& $info
    
	echo ":0" >> $mailrc
	echo "* ^Subject.*\/sendmail.*" >> $mailrc
	echo "| cat >> $matchmailfile;"' x=`date +%s`; echo $''x'' $'"MATCH >> $ftplogfile" >> $mailrc
	
	echo >> $mailrc
	echo ":0" >> $mailrc
	echo "$defaultmailbox" >> $mailrc
	
    echo "Install the new procmailrc $mailrc" >>& $info
	sudo cp -f $mailrc /etc/procmailrc >>& $info

    #echo "Install the new got_file :$ftplogfile" >>& $info
    #echo "#\!/bin/csh" >  $dtn2testroot/mail/got_file.sh
    #echo "set ftplogfile=$ftplogfile" >> $dtn2testroot/mail/got_file.sh
    #echo "set defaultmailbox=$defaultmailbox" >> $dtn2testroot/mail/got_file.sh
    #cat  $dtn2testroot/mail/got_file-template.sh >> $dtn2testroot/mail/got_file.sh
    #chmod +x $dtn2testroot/mail/got_file.sh
endif



echo "Changing the queue check time for sendmail ... " >>& $info
set sysconf=$logroot/sendmail
echo "DAEMON=yes" > $sysconf
echo "QUEUE=10s" >> $sysconf
sudo cp -f $sysconf /etc/sysconfig/sendmail

echo "Restarting sendmail ... " >>& $info
sudo /etc/init.d/sendmail restart  >>& $info

#send all the mail now
if ($id == 1) then
    echo "Sending the mail now ...."  >>& $info
    tclsh $dtn2testroot/mail/send-files.tcl $txdir $ftplogfile $info $dest_addr 
endif

    


 
