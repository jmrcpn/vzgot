#!/bin/sh
#
# vzgot		This shell script takes care of starting and stopping
#               vzgot container with the "ON" status
#
# description:	Scan vzgot vdir directory to find out about container
#		and start them if requested.
#
# chkconfig: - 98 2
#   description: Startup/shutdown vzgot containers
### BEGIN INIT INFO
# Provides: vzgot
# Required-Start: network
# Required-Stop:
# Default-Start: 
# Default-Stop: 0 1 6
# Short-Description: starting container
### END INIT INFO

echo "Obsolete INIT, look at ok_init"
exit 1


#basic function
[ -f  /etc/rc.d/init.d/functions ] && . /etc/rc.d/init.d/functions

CONFDIR=/etc/vzgot
if [ -f $CONFDIR/vzgot_config ] ; then
  . $CONFDIR/vzgot_config
  fi

#------------------------------------------------------------------
vzgotstart()

{
modprobe br_netfilter
echo 1 > /proc/sys/net/bridge/bridge-nf-call-iptables
echo -n "Vzgot starting" && success || failure
#loading bridge filtering module
echo
count=0
for cont in `ls $CONFDIR/names`
  do
  . $CONFDIR/names/$cont
  onboot=`echo $ONBOOT | tr [:upper:] [:lower:]`
  if [ "$onboot" != "yes" ] ; then
    continue
    fi
  CONTPID=""
  if [ -f $VZLIB/vzdir/$cont/first.pid ] ; then
    CONTPID=`cat $VZLIB/vzdir/$cont/first.pid`
    fi
  if [ -n "$CONTPID" -a -e /proc/$CONTPID ] ; then
    continue
    fi
  BOOTLOG=$VZLIB/vzdir/$cont/bootlog
  date > $BOOTLOG
  echo -n "Booting container $cont"
  vzgot boot $cont && success || failure
  echo
  count=`expr $count + 1`
  sleep 1
  done
echo -n "vzgot: $count container started" && success || failure
echo 
}

vzgotstop()

{
echo -n "Vzgot stopping" && success || failure
echo
for cont in `ls $CONFDIR/names`
  do
  . $CONFDIR/names/$cont
  CONTPID=""
  if [ -f $VZLIB/vzdir/$cont/first.pid ] ; then
    CONTPID=`cat $VZLIB/vzdir/$cont/first.pid`
    fi
  if [ -n "$CONTPID" -a -e /proc/$CONTPID ] ; then
    echo -n "Halting container $cont"
    vzgot shutdown $cont && success || failure
    echo
    while [ -f /$VZLIB/vzdir/$cont/first.pid ] ; do
      sleep 1
      done
    fi
  rm -fr $VZLIB/vzdir/$cont/first.pid
  done
echo -n "vzgot: all container now down" && success || failure
echo
}

#------------------------------------------------------------------
case "$1" in
  start)
	# Start daemons.
	vzgotstart
        
	;;

  stop)
	# Stopping daemons.
	vzgotstop
	;;

  restart)
	$0 stop
	$0 start
	;;

  reload)
	echo "Nothing to do"
	;;

  status)
	status $PROG
	;;
  *)
	echo "Usage: vzgot {start|stop|restart|status}"
	exit 1
esac

exit 0

