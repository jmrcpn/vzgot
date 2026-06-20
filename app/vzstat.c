// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*	TSTONL is a small test program to be    */
/*      run on a HOST to see the online template*/
/*      on the console.                         */
/*      Main purpose is to check online display */
/*      for a specific container.               */
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
#include	"subcfg.h"
#include	"unicfg.h"
#include	"unicnt.h"
#include	"unilck.h"

#define VZSTAT  "vzstat"

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
		      "[-d confdir] "
		      "[-d debug] "
		      "[-r ] "
		      "[-h ] "
		      "[-v ] "
		      "container\n",appname);
(void) fprintf(stderr,"\t\t-c confdir   : use 'confdir' instead of dir '%s'\n,",
                                          apl_dfltconfdir());
(void) fprintf(stderr,"\t\t-d debug     : Debug level\n");
(void) fprintf(stderr,"\t\t-r           : Display container status every sec\n");
(void) fprintf(stderr,"\t\t-h           : display usage\n");
(void) fprintf(stderr,"\t\t-v           : verbose debug\n");
(void) fprintf(stderr,"\t\tcontainer    : container name to display infomation\n");
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
#define CSI             "["
#define HIDECUR         "[?25l"
#define SHOWCUR         "[?25h"
#define POS00           "[1;1H"
#define CLEARSCR        "[0J"

int status;
char c;
const char *contname;
TPLPTR *tpl;
char *confdir;
_Bool repeat;
_Bool proceed;
int phase;

appname=VZSTAT;
tpl=(TPLPTR *)0;
confdir=(char *)0;
repeat=false;
(void) apl_trapsegv(true);
(void) openlog(appname,LOG_NDELAY|LOG_PID,LOG_DAEMON);
argv=prc_preptitle(argc,argv,environ);
foreground=true;
status=0;
while ((c=getopt(argc,argv,"+c:d:rvh"))!=EOF) {
  switch(c) {
    case   'c'  :
      confdir=apl_freestr(confdir);
      confdir=strdup(optarg);
      break;
    case   'd'  :
      debug=atoi(optarg);
      break;
    case   'r'  :
      repeat=true;
      break;
    case   'v'  :
      verbose=true;
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
  //(void) log_alert(0,"%s JMPDBG phase='%d' contname=<%s>",appname,phase,contname);
  switch (phase) {
    case 0      :       //do we have a container name
      if (argc<=optind) {
        (void) usage();
        phase=999;      //No container!
        }
      break;
    case 1      :       //loading container config
      contname=argv[optind];
      if (cfg_loadconfig(confdir,contname)!=0) 
        phase=999;      //Unable to load container configuration?
      break;
    case 2      :       //loading the container
      if ((tpl=cfg_open_online(confdir,contname,getenv(ONLINETPL)))==(TPLPTR *)0) {
        (void) log_alert(0,"%s unable to open <%s> template within config",
                            appname,getenv(ONLINETPL));
        phase=999;      //trouble trouble
        }
      break;
    case 3      :       //init display
      if (repeat==true) {
        (void) fprintf(stdout,"%s%s%s",HIDECUR,POS00,CLEARSCR);
        (void) fflush(stdout);
        }
      break;
    case 4      :       //display online information
      if (repeat==true) {
        (void) fprintf(stdout,"%s",POS00);
        (void) fflush(stdout);
        }
      (void) cfg_update_online(contname,tpl);
      if (cfg_flush_online(stdout,tpl)==false)
        break;          //exiting the repeat loop
      (void) fprintf(stdout,"%s-%s: CTRL-\\ to exit\n",appname,apl_getvers());
      if (repeat==true) {
        (void) sleep(1);
        (void) apl_checksig();
        if ((sigterm==false)&&(sigquit==false))
          phase--;      //lets do it again
        }
      break;
    case 5      :       //closing the tpl structure
      if (repeat==true) {
        (void) fprintf(stdout,"%s",SHOWCUR);
        (void) fflush(stdout);
        }
      tpl=cfg_close_online(tpl);
      break;
    default     :       //SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
confdir=apl_freestr(confdir);
argv=prc_cleantitle();
(void) closelog();
(void) apl_settrap(false);
(void) apl_trapsegv(false);
exit(status);
}
