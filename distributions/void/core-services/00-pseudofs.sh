#----------------------------------------------------------------------
#Vzgot void configuration file to be used by vzgot container
#----------------------------------------------------------------------
#mounting all devices defined within /etc/fstab
msg "Mounting according /etc/fstab..."
/usr/bin/mount -a

#setting container HOSTNAME
if [ -f /etc/hostname ] ; then
  /usr/sbin/hostname `cat /etc/hostname`
fi


#----------------------------------------------------------------------
