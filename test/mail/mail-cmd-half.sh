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
set dest_addr = "rabin@$final_ip"
    
#create the sendmail.mc file
echo "Copying the sendmail template ... and copying to $logroot/sendmail-$id.mc .." >>& $info
sudo cp -f $dtn2testroot/mail/sendmail-template.mc $logroot/sendmail-$id.mc >>& $info
sed -i "s/__SMART_HOST__/$next_relay/g" $logroot/sendmail-$id.mc  >>& $info
sed -i "s/__LOCAL_DOMAIN__/$local_mail_domain/g" $logroot/sendmail-$id.mc  >>& $info

#install the new mc file
echo "Install the new sendmail file ..." >>& $info
sudo m4 $logroot/sendmail-$id.mc > $logroot/sendmail-$id.cf 
sudo cp -f $logroot/sendmail-$id.cf /etc/mail/sendmail.cf
sudo /etc/init.d/sendmail restart  >>& $info


#install the .procmailrc file
echo "Install the new procmailrc file ..." >>& $info
cp $dtn2testroot/mail/procmailrc /users/rabin/.procmailrc  >>& $info


#install the got_file file
echo "Install the new got_file :$ftplogfile" >>& $info
echo "#\!/bin/csh" >  $dtn2testroot/mail/got_file.sh
echo "set ftplogfile=$ftplogfile" >> $dtn2testroot/mail/got_file.sh
cat  $dtn2testroot/mail/got_file-template.sh >> $dtn2testroot/mail/got_file.sh
chmod +x $dtn2testroot/mail/got_file.sh

#send all the mail now
if ($id == 1) then
    echo "Sending the mail now ...."  >>& $info
    tclsh $dtn2testroot/mail/send-files.tcl $txdir $ftplogfile $info $dest_addr 
endif

    


 
