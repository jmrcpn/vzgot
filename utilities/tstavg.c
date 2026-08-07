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
#include	"lowapl.h"
#include	"lowtyp.h"
#include	"utlapl.h"
#include	"utlprc.h"
#include	"subcfg.h"
#include	"subprc.h"
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

*/
/************************************************/
/*						*/
/*	procedure to extract loadavg values from*/
/*	cgroup data, format then and write them	*/
/*	to the loadavg file.			*/
/*						*/
/************************************************/
static const char *cal_loadavg(const char *contname,uint16_t nbr_cpu,double delta_t)

{
#define OPEP	"unicnt.c:cal_loadavg,"
#define	MDELTA	0.1	//minimal delta between to measurement

static char strload[100];
static u_vlong last_host_load=0;
static u_vlong last_cnt_load=0;

u_vlong usage;
u_vlong pression;
u_vlong cur_host_load;
u_vlong cur_cnt_load;
double host_avg[3];
uint32_t pids_current;
pid_t last_pid;
double ratio;
int phase;
_Bool proceed;

(void) memset(strload,'\000',sizeof(strload));
usage=(u_vlong)0;
pression=(u_vlong)0;
cur_host_load=(u_vlong)0;
cur_cnt_load=(u_vlong)0;
ratio=0.0;
pids_current=0;
last_pid=(pid_t)0;
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d' delta_t='%lf'",OPEP,phase,delta_t);
  switch (phase) {
    case 0	:	//getting the current container usage component
      if (prc_cnt_usage(contname,&usage)==false) {
        (void) log_alert(0,"%s Unable to get container <%s> cpu usage (Bug?)",
			    OPEP,contname);
	phase=999;	//Trouble trouble
	}
      if (prc_cnt_pressure(contname,&pression)==false) {
        (void) log_alert(0,"%s Unable to get container <%s> cpu pressure (Bug?)",
		            OPEP,contname);
  	phase=999;	//Trouble trouble
	}
      cur_cnt_load=usage+pression;
      break;
    case 1	:	//getting the current HOST usage component
      if (prc_cnt_usage("",&usage)==false) {
        (void) log_alert(0,"%s Unable to get HOST cpu usage (Bug?)",OPEP);
	phase=999;	//Trouble trouble
	}
      if (prc_cnt_pressure("",&pression)==false) {
        (void) log_alert(0,"%s Unable to get HOST cpu pressure (Bug?)",OPEP);
  	phase=999;	//Trouble trouble
	}
      cur_host_load=usage+pression;
      break;
    case 2	:	//Firt time calculation?
      if (last_host_load==(u_vlong)0) 
	phase=999;	//We need at least one pass to compute ratio
      break;
    case 3	:	//getting the total number of pid own by  container
      if (prc_cnt_pids_current(contname,&pids_current)==false) {
        (void) log_alert(0,"%s Unable to get <%s> current number of pids (Bug?)",
		            OPEP,contname);
  	phase=999;	//Trouble trouble
	}
      break;
    case 4	:	//getting the official LOAD Usage.
      if (prc_host_loadavg(&host_avg[0],&host_avg[1],&host_avg[2])==false) {
        (void) log_alert(0,"%s Unable to get HOST current load (Bug?)",OPEP);
  	phase=999;	//Trouble trouble
	}
      break;
    case 5	:	//computing ration container/HOST
      if (delta_t>MDELTA) { //always
	double delta_host;
	double delta_cnt;

	delta_cnt=cur_cnt_load-last_cnt_load;
	delta_host=cur_host_load-last_host_load;
	if (delta_host>0.0) {	//should be always the case
	  ratio=delta_cnt/delta_host;
	  if (ratio>1.0)
	    ratio=1.0;
	  break; 		//Ne need to go further
	  }
	}
      if (ratio<0.0) {
        (void) log_alert(0,"%s Beware load ratio='%f' (expected>0.0 Bug?)",
			    OPEP,ratio);
	phase=999;		//Trouble!
	}
      break;
    case 6	:	//Getting the CONTAINER lastpid
      if (sys_get_last_pid(contname,&last_pid)==false) {
        (void) log_alert(0,"%s Unable to get container last pid (Bug?)",OPEP);
	phase=999;	//trouble trouble
	}
      break;
    case 7	:	//applying ratio
      for (int i=0;i<3;i++) 
	host_avg[i]*=ratio;
      (void) snprintf(strload,sizeof(strload),"%5.2lf %5.2lf %5.2lf 1/%u %u",
			        	       host_avg[0],
					       host_avg[1],
					       host_avg[2],
					       pids_current,last_pid);
      break;
    default	:	//SAFE Guard
      last_cnt_load=cur_cnt_load;
      last_host_load=cur_host_load;
      proceed=false;
      break;
    }
  phase++;
  }
return (const char *)strload;

#undef	MDELTA
#undef	OPEP
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
