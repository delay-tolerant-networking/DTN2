#!/bin/csh
if ($#argv != 6) then
    echo "Usage: Sorry no help . see fileitself"
    exit
endif


# Now generate all the sendmail.mc files
# Now generate daemon for the node
    
set idplus  = `expr $id - 1`
set idminus = `expr $id + 1`
    
set local_ip = node-$id-link-$idminus
set next_ip  = node-$idplus-link-$id
set start_ip = node-1
set final_ip = node-$maxnodes
set local_mail_domain = node-$id-link-2.emulab.net
set dest_addr = "rabin@$final_ip"
    
#create the sendmail.mc file
echo "Copying the sendmail template ..." >>& $info
cp -f $dtn2testroot/mail/sendmail-template.mc > $logroot/sendmail-$id.mc >>& $info
sed -i 's/__SMART_HOST__/$next_ip/g' $logroot/sendmail-$id.mc  >>& $info
sed -i 's/__LOCAL_DOMAIN__/$local_mail_domain/g' $logroot/sendmail-$id.mc  >>& $info

#install the new mc file
echo "Install the new sendmail file ..." >>& $info
sudo m4 $logroot/sendmail-$id.mc > /etc/mail/sendmail.mc  >>& $info
sudo /etc/init.d/sendmail restart  >>& $info


#install the .procmailrc file
echo "Install the new procmailrc file ..." >>& $info
cp $dtn2testroot/mail/procmailrc $HOME/.procmailrc  >>& $info

#install the got_file file
echo "Install the new got_file ..." >>& $info
cp $dtn2testroot/mail/got_file-template.sh $dtn2testroot/mail/got_file.sh
sed -i 's/__FTPLOG_FILE__/$ftplogfile/g' $dtn2testroot/mail/got_file.sh


#send all the mail now
if ($id == 1) then
    echo "Sendin the mail now ...."  >>& $info
    tclsh $dtn2testroot/mail/send-files.tcl $txdir $ftplogfile $info $dest_addr 
endif

    


 
