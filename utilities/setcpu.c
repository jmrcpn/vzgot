// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*	SETCPU is a small test program to be    */
/*      run on a HOST to change the CPU         */
/*      assignement for a specific container.   */
/*						*/
/************************************************/
#include	<linux/reboot.h>
#include	<linux/sched.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<sched.h>
#include	<stdio.h>
#include	<string.h>
#include	<syslog.h>
#include	<signal.h>
#include	<stdlib.h>
#include	<time.h>
#include	<sched.h>
#include	<sys/capability.h>
#include	<sys/mount.h>
#include	<sys/mman.h>
#include	<sys/personality.h>
#include	<sys/prctl.h>
#include	<sys/reboot.h>
#include	<sys/sysinfo.h>
#include	<sys/utsname.h>
#include	<sys/wait.h>

#include	"dbglog.h"
#include	"lowtyp.h"
#include	"utlapl.h"
#include	"utlprc.h"
#include	"utlsys.h"
#include	"subcfg.h"
#include	"subprc.h"
#include	"unilck.h"
#include	"unicnt.h"

#define CHKCPU "cpu_assign"

/*

*/
/************************************************/
/*						*/
/*	Procedure to display the program usage	*/
/*						*/
/************************************************/
static void usage()

{
(void) fprintf(stderr,"%s Version %s\n",appname,apl_getvers());
(void) fprintf(stderr,"usage:  %s "
		      "[-d debug] "
		      "[-h ] "
		      "[-r ] "
		      "[-v ] "
		      "container_name \n",appname);
(void) fprintf(stderr,"\t\t-d debug     : Debug level\n");
(void) fprintf(stderr,"\t\t-h           : display usage\n");
(void) fprintf(stderr,"\t\t-r           : set cpu asignment ratio\n");
(void) fprintf(stderr,"\t\t-v           : verbose debug\n");
(void) fprintf(stderr,"\t\tname_name    : container name to probe\n");
}
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
#define PACE 5.0

int status;
char c;
char *ratio;
_Bool proceed;
int phase;

status=0;
appname=CHKCPU;
ratio=strdup("100.0");
foreground=true;
(void) apl_trapsegv(true);
(void) openlog(appname,LOG_NDELAY|LOG_PID,LOG_DAEMON);
argv=prc_preptitle(argc,argv,environ);
while ((c=getopt(argc,argv,"+d:r:vh"))!=EOF) {
  switch(c) {
    case   'd'  :
      debug=atoi(optarg);
      break;
    case   'v'  :
      verbose=true;
      break;
    case   'r'  :
      ratio=apl_freestr(ratio);
      ratio=strdup(optarg);
      break;
    case   'h'  :
    default	:
      status--;
      (void) usage();
      break;
    }
  }
phase=0;
proceed=(status==0);
(void) apl_settrap(true);
while (proceed==true) {
  switch (phase) {
    case 0      :       //do we have a container name
      if (argc<=optind) {
        (void) usage();
        phase=999;      //No container!
        }
      break;
    case 1      :       //loading container config
      if (cfg_loadconfig((const char *)0,argv[optind])!=0) 
        phase=999;      //Unable to load container configuration?
      break;
    case 2      :       //Let proceed with display
      if (sys_setcpuset(argv[optind],apl_getdouble(ratio))==false) {
        (void) fprintf(stdout,"Unable to assign CPU with ratio=<%s> (Bug?)",ratio);
        status=1;
        }
      break;
    default     :       //SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
(void) apl_settrap(false);
argv=prc_cleantitle();
(void) closelog();
(void) apl_trapsegv(false);
ratio=apl_freestr(ratio);
exit(status);
}
