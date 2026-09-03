#!/bin/sh
#
# vzgot		This shell script takes care of starting and stopping
#               vzgot container with the "ON" status
#
# description:	Startup/shutdown vzgot containers in ONBOOT="yes" mode
#
# chkconfig: - 98 2
#
### BEGIN INIT INFO
# Provides: vzgot
# Required-Start: network
# Required-Stop:
# Default-Start: 2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: starting containers
### END INIT INFO

MODE="sysv"
# THis script could be used by systemd, lets find out
if [ -n "$NOTIFY_SOCKET" ] || { [ -x /usr/bin/systemctl ] && /usr/bin/systemctl is-system-running >/dev/null 2>&1; }; then
  MODE="systemd"
fi

#ANSI colore to console display (Osukiss sysv)
GREEN="\033[1;32m"
RED="\033[1;31m"
NORMAL="\033[0m"

# log_status $1 = message, $2 = Return status (0 ou 1)
log_status()

{
case "${MODE}" in
  "sysv"	)
    if [ "$2" = "0" ]; then
      printf "[  ${GREEN}OK${NORMAL}  ] %s\n" "$1"
    else
      printf "[${RED}FAILED${NORMAL}] %s\n" "$1"
    fi
    ;;
  "systemd"	)
    if [ "$2" = "0" ]; then
      echo "<6>vzgot: $1"
    else
      echo "<3>vzgot: ERROR - $1"
    fi
    ;;
  *		)
    echo $1 -> $2
    ;;
  esac
}

#10000 cgroup cpu.weight maximun value
WEIGHT=10000

CGROUP="/sys/fs/cgroup"
VZGROUP=${CGROUP}/vzgot 

CONFDIR=/etc/vzgot
if [ -f $CONFDIR/vzgot_config ] ; then
  . $CONFDIR/vzgot_config
  fi

#------------------------------------------------------------------
vzgotstart()

{
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
log_status "vzgot: ${count} containers are now on the 'starting block'" 0
return 0;
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
log_status "Stopping all ${count} containers ASAP" 0
while [ "$t" -lt "$MAXT" ]; 
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
  log_status "Waiting containers full shutdown, remain $remain/$count...." 0
  sleep 3
  done
ret=0;
if [ ${remain} = 0 ] ; then
  log_status "vzgot: all containers now down" ${ret};
else
  ret=1;
  log_status "vzgot: $remain containers still up" ${ret}
fi
return ${ret}
}

#------------------------------------------------------------------
ret=0;
case "$1" in
  start		)
	#make sure to have a good sysctl
	sysctl --system -q > /dev/null
	#make sure to have a we have the fuse device
	modprobe fuse
	# mounting  loop device (if any)
        mount -a -O loop 2>/dev/null || true
	#cgroup configuration
	#Caution! # CONFIG_RT_GROUP_SCHED need to be set in kernel config
	if [ -f ${CGROUP}/cgroup.controllers ] ; then
	  #Caution! # CONFIG_RT_GROUP_SCHED need to be set in kernel config
	  echo "+cpu +cpuset +memory +pids" >${CGROUP}/cgroup.subtree_control
	  fi
	mkdir -p ${VZGROUP}
	echo ${WEIGHT} > ${VZGROUP}/cpu.weight
	if [ -f ${VZGROUP}/cgroup.controllers ] ; then
	  echo "+cpu +cpuset +memory +pids" >${VZGROUP}/cgroup.subtree_control
	  fi
	# Start daemons.
	vzgotstart;
	ret=$?;
	;;

  stop		)
	# Stopping daemons.
	vzgotstop;
	ret=$?;
	;;

  restart	)
	$0 stop
	$0 start
	;;

  reload	)
	echo "Nothing to do"
	;;

  force-reload	)
	echo "Nothing to do"
	;;

  status	)
	status $PROG
	;;
  *		)
	echo "Usage: vzgot {start|stop|restart|reload|force-reload|status}"
	exit 1
esac

exit ${ret}

