// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*	LOADAVG is a small test program to be   */
/*      run on a HOST to see the loadavg (used  */
/*      by uptime) of a specific container      */
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
#include	"unicnt.h"
#include	"unilck.h"

#define CHKLOAD "loadavg"

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
		      "[-c confdir] "
		      "[-d debug] "
		      "[-n nbrcpu] "
		      "[-t wakup] "
		      "[-f ] "
		      "[-h ] "
		      "[-v ] "
		      "container_name\n",appname);
(void) fprintf(stderr,"\t\t-c confdir   : use 'confdir' instead of dir '%s'\n,",
                                          apl_dfltconfdir());
(void) fprintf(stderr,"\t\t-d debug     : Debug level\n");
(void) fprintf(stderr,"\t\t-n nbrcpu    : Pretend the container is using nbrcpu\n");
(void) fprintf(stderr,"\t\t-t sec       : Display container loadavg every sec\n");
(void) fprintf(stderr,"\t\t-h           : display usage\n");
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
char *confdir;
TIMETIC last_time;
TIMETIC cur_time;
double delta_t;
int nbrcpu;
_Bool proceed;
int phase;
u_int wakeup=5;   //wakeup time in second to display
                  //container loadavg

appname=CHKLOAD;
delta_t=(double)0.0;
confdir=(char *)0;
nbrcpu=sysconf(_SC_NPROCESSORS_CONF);
foreground=true;
status=0;
(void) apl_trapsegv(true);
(void) openlog(appname,LOG_NDELAY|LOG_PID,LOG_DAEMON);
argv=prc_preptitle(argc,argv,environ);
while ((c=getopt(argc,argv,"+c:d:n:t:vh"))!=EOF) {
  switch(c) {
    case   'c'  :
      confdir=apl_freestr(confdir);
      confdir=strdup(optarg);
      break;
    case   'd'  :
      debug=atoi(optarg);
      break;
    case   'n'  :
      if (atoi(optarg)<nbrcpu)
        nbrcpu=atoi(optarg);
      break;
    case   't'  :
      wakeup=atoi(optarg);
      if (wakeup<PACE)
        wakeup=(u_int)PACE;
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
  switch (phase) {
    case 0      :       //do we have a container name
      if (argc<=optind) {
        (void) usage();
        phase=999;      //No container!
        }
      break;
    case 1      :       //loading container config
      if (cfg_loadconfig(confdir,argv[optind])!=0) 
        phase=999;      //Unable to load container configuration?
      break;
    case 2      :       //Let proceed with display
      (void) fprintf(stdout,"starting to wakeup every %d second\n",wakeup);
      (void) clock_gettime(CLOCK_MONOTONIC,&last_time);
      (void) cal_loadavg(argv[optind],nbrcpu,PACE);
      break;
    case 3      :       //Let proceed with display
      (void) sleep(wakeup);
      (void) clock_gettime(CLOCK_MONOTONIC,&cur_time);
      delta_t=(double)(cur_time.tv_sec-last_time.tv_sec)+
              (double)((cur_time.tv_nsec-last_time.tv_nsec)/1e9);
      (void)apl_checksig();
      if ((sigquit==true)||(sigterm==true)) {
        (void) fprintf(stdout,"signal received, exiting\n");
        break;
        }
      if (delta_t>=PACE) { //its time to update /proc counter
        const char *data;

        data=cal_loadavg(argv[optind],nbrcpu,delta_t);
        (void) fprintf(stdout,"%s cpu=%d loadvag= %s\n",argv[optind],nbrcpu,data);
        }
      last_time=cur_time;
      phase--;        //looping
      break;
    default     :       //SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
confdir=apl_freestr(confdir);
(void) apl_settrap(false);
argv=prc_cleantitle();
(void) closelog();
(void) apl_trapsegv(false);
exit(status);
}
