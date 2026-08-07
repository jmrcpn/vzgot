// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*	This programe purpose is to be the PID 2*/
/*      within the container to check special   */
/*      mounting within /proc.                  */
/*						*/
/************************************************/
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <poll.h>
#include <ftw.h>

#include        <sys/prctl.h>
#include        <syslog.h>
#include        <time.h>

#include	"version.h"
#include	"dbglog.h"
#include	"lowapl.h"
#include	"utlapl.h"
#include	"utlprc.h"
#include	"utlsys.h"
#include	"unicnt.h"
#include	"unimon.h"
#include	"subcfg.h"

#define PRG             "main"

#define	APPNAME	        "vzmon"
/*
^L
*/
/************************************************/
/*						*/
/*	Program Entry.				*/
/*						*/
/************************************************/
int main(int argc,char *argv[]) 

{
int status;

appname=APPNAME;
status=0;
(void) apl_trapsegv(true);
(void) apl_settrap(true);
(void) mon_monitoring();
(void) apl_settrap(false);
(void) apl_trapsegv(false);
return status;
}
