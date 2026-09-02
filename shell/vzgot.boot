#!/usr/bin/env bash
#-------------------------------------------------------------------
#procedure to install a specific VZserver 
#$1 is the vzserver container name
#$2 is the distribution
#-------------------------------------------------------------------
if [ -z "$1" -o -z "$2" ] ; then
  echo "You need to specify contname and distribution"
  echo "commande is $0 container_name distribution"
  exit 1
  fi

CONTNAME=$1
DISTRIB=$2;

#===================================================================
#lock procedure
lockproc()

{
LOCKFILE=/var/run/`basename $0`.$1.pid

trap "rm -f $LOCKFILE ; exit 1" 1 2 3 15
if (set -C; : > $LOCKFILE) 2> /dev/null ; then
    echo $$ >> $LOCKFILE
  else
    echo "Another '`basename $0` $1' is running, existing at once!"
    echo "remove $LOCKFILE, if it is not the case"
    rm -f $LOCKFILE
    exit 0;
  fi
}
#-------------------------------------------------------------------
#abort procedure
abort ()

{
echo "$1"
rm -f $LOCKFILE
exit 1;
}

#===================================================================
#main process

#locking checking rootfs process
lockproc $CONTNAME

#===================================================================
#extract the configuration
CONFDIR=/etc/vzgot
. ${CONFDIR}/vzgot_config

if [ ! -f ${CONFDIR}/names/${CONTNAME} ] ; then
  echo "Missing ${CONTNAME} name file"
  exit 1	#trouble trouble
fi

. ${CONFDIR}/names/${CONTNAME}
if [ -z "${HOMEFS}" ] ; then 
  echo "Missing ${CONTNAME} HOMEFS variable"
  exit 1	#trouble trouble
fi

#===================================================================
#Making sure devref is populated

DEVREF=${HOMEFS}/devref
if [ ! -d ${DEVREF} ] ; then
  mkdir -p -m 755 ${DEVREF}/pts
  (
  cd /dev
  rsync -a 		\
	console		\
	full		\
	fuse		\
	mem		\
	null		\
	ptmx		\
	random		\
	stderr		\
	stdin		\
	stdout		\
	tty		\
	urandom		\
	zero		\
	loop[0-7]	\
	${DEVREF}
  )
fi

#===================================================================
#network setup from HOST standpoin
#setting distribution
case "${DISTRIB}" in
  "alpine"	)
    FLINUX="Alpine"	#Alpine family distribution
    ;;
  "debian"	)
    FLINUX="Debian"	#Debian family distribution
    ;;
  "devuan"	)
    FLINUX="Devuan"	#Devuan family distribution
    ;;
  "fedora"	)
    FLINUX="Fedora"	#Fedora family distribution
    ;;
  "gentoo"	)
    FLINUX="Gentoo"	#Gentoo family distribution
    ;;
  "ok-1"	)
    FLINUX="Osukiss"	#Osukiss family distribution
    ;;
  "opensuse"	)
    FLINUX="Opensuse"	#opensuse distribution
    ;;
  "ubuntu"	)
     FLINUX="Ubuntu"	#Ubuntu family distribution
    ;;
  "void"	)
    FLINUX="Void"	#Void family distribution
    ;;
  "*"		)
    abort "Unknown Linux distribution, exiting"
    exit 1 
    ;;
  esac;
#===================================================================
#let do distribution specific confifiguration
if [ -x `dirname $0`/vzgot.boot.${FLINUX} ] ; then
  `dirname $0`/vzgot.boot.${FLINUX} ${CONTNAME}
else
   echo "`dirname $0`/vzgot.boot for ${DISTRIB} is missing"
  true
fi

RETVAL=$?
if [ ${RETVAL} -ne 0 ] ; then 
  abort "Unable to prepare ${CONTNAME} for boot"
fi
#===================================================================
rm -f $LOCKFILE
