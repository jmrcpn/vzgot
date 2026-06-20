#!/usr/bin/bash
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
# Default-Start: 3 4 5
# Default-Stop: 0 1 2 6
# Short-Description: starting container
### END INIT INFO


#basic function
. /lib/lsb/init-functions

#10000 cgroup cpu.weight maximun value
WEIGHT=10000

CGROUP="/sys/fs/cgroup"
CONFDIR=/etc/vzgot
if [ -f $CONFDIR/vzgot_config ] ; then
  . $CONFDIR/vzgot_config
  fi

#------------------------------------------------------------------
vzgotstart()

{
#modprobe br_netfilter
#echo 1 > /proc/sys/net/bridge/bridge-nf-call-iptables

count=0
for cont in `ls $CONFDIR/names`
  do
  . $CONFDIR/names/$cont
  onboot=`echo $ONBOOT | tr [:upper:] [:lower:]`
  if [ "$onboot" != "yes" ] ; then
    continue
    fi
  CONTDIR=$VZDIR/$cont
  if [ -n "$HOMEFS" ] ; then
    CONTDIR=$HOMEFS
    fi
  CONTPID=""
  if [ -f $CONTDIR/first.pid ] ; then
    CONTPID=`cat $CONTDIR/first.pid`
    fi
  if [ -n "$CONTPID" -a -e /proc/$CONTPID ] ; then
    continue
    fi
  delay=`expr  $count \* 3  + 10`
  # log_info_msg "$delay seconds delay to boot container $cont"
  (
  sleep $delay
  BOOTLOG=$CONTDIR/bootlog
  vzgot boot $cont > $BOOTLOG 2>&1
  date >> $BOOTLOG
  ) &
  # evaluate_retval
  count=`expr $count + 1`
  done
log_info_msg "vzgot: $count containers are now on the 'starting block'"
true
evaluate_retval
}

vzgotstop()

{
MAXT=30
count=0
tostop=`ls $CONFDIR/names | sort -r`
for cont in ${tostop}
  do
  . $CONFDIR/names/$cont
  CONTDIR=$VZDIR/$cont
  if [ -n "$HOMEFS" ] ; then
    CONTDIR=$HOMEFS
    fi
  CONTPID=""
  if [ -f $CONTDIR/first.pid ] ; then
    CONTPID=`cat $CONTDIR/first.pid`
    fi
  if [ -n "$CONTPID" -a -e /proc/$CONTPID ] ; then
    sleep 0.2
    vzgot shutdown $cont &
    count=`expr $count + 1`
    fi
  done
log_info_msg "Stopping all $count containers ASAP"
sleep 1
evaluate_retval
for ((t=1;t<$MAXT;t++))
  do
  remain=0;
  for cont in ${tostop}
    do
    . $CONFDIR/names/$cont
    CONTDIR=$VZDIR/$cont
    if [ -n "$HOMEFS" ] ; then
      CONTDIR=$HOMEFS
      fi
    CONTPID=""
    if [ -f $CONTDIR/first.pid ] ; then
      CONTPID=`cat $CONTDIR/first.pid`
      fi
    if [ -n "$CONTPID" -a -e /proc/$CONTPID ] ; then
      remain=`expr $remain + 1`
      fi
    done
  if [ $remain = 0 ] ; then
    break;
    fi
  log_info_msg "Waiting containers full shutdown, remain $remain/$count...."
  sleep 3
  evaluate_retval
  done
if [ $remain = 0 ] ; then
  log_info_msg "vzgot: all containers now down"
  rmdir $VZCGROUP/containers/
  rmdir $VZCGROUP/supervisors/
  rmdir $VZCGROUP/
  true
else
  log_info_msg "vzgot: $remain containers still up"
  false
fi
evaluate_retval
}

#------------------------------------------------------------------
case "$1" in
  start)
	#make sure to have a good sysctl
	sysctl --system -q > /dev/null
	#make sure to have a we have the fuse device
	modprobe fuse
	#cgroup configuration
	if [ -f ${CGROUP}/cgroup.controllers ] ; then
	  #Caution! # CONFIG_RT_GROUP_SCHED is not set
	  echo "+cpu +cpuset +memory +pids" >${CGROUP}/cgroup.subtree_control
	  fi
	mkdir -p ${VZCGROUP}
	if [ -f ${VZCGROUP}/cgroup.controllers ] ; then
	  echo "+cpu +cpuset +memory +pids"		\
	      > ${VZCGROUP}/cgroup.subtree_control
	  fi
	mkdir -p ${VZCGROUP}/containers
	if [ -f ${VZCGROUP}/containers/cgroup.controllers ] ; then
	  #Caution! # CONFIG_RT_GROUP_SCHED need to be set 
	  echo "+cpu +cpuset +memory +pids"		\
	      > ${VZCGROUP}/containers/cgroup.subtree_control
	  fi
	echo ${WEIGHT} > ${VZCGROUP}/containers/cpu.weight
	mkdir -p ${VZCGROUP}/supervisors
	# Start daemons.
	vzgotstart;
	;;

  stop)
	# Stopping daemons.
	vzgotstop;
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

