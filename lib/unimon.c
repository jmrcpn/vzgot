// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*	UNIMON:					*/
/*      Module to implement container monitoring*/
/*      within the container itslef.            */
/*						*/
/************************************************/
#include	<errno.h>
#include	<fcntl.h>
#include	<poll.h>
#include	<signal.h>
#include	<stdio.h>
#include	<string.h>
#include	<time.h>


#include	"dbglog.h"
#include	"lowapl.h"
#include	"utlsys.h"
#include	"unimon.h"

#define PRG     "unimon.c"



volatile sig_atomic_t keep_running = true;
/*
^L
*/
/************************************************/
/*						*/
/*	Procedure to trap a shutdown signal     */
/*						*/
/************************************************/
static void handle_shutdown(int sig)

{
keep_running=false;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to cont binding request and	*/
/*	decide if this still acceptable.	*/
/*						*/
/************************************************/
static _Bool check_binding()

{
#define	OPEP	PRG":check_binding"

#define	MXTRY	50
#define	TRYGAP  60*30	//MXTRY reconnect within 30 minutes tolerance

static time_t last[MXTRY];
static int count;

_Bool isok;
time_t curtime;
int phase;
_Bool proceed;

isok=false;
curtime=time((time_t *)0);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//purging count
      while ((count>0)&&(last[0]<=(curtime-TRYGAP))) {
	count--;
	if (count>0) 
	  (void)memmove(last,last+1,sizeof(time_t)*count);
	last[count]=0;
	}
      if (count<(MXTRY-1))
	phase++;	//no need to check time AND try
      break;
    case 1	:	//maximun try reach within TRYGAP
      if (last[0]>(curtime-TRYGAP)) {
	(void) log_alert(0,"%s, too many binding within  container",OPEP);
	(void) log_alert(0,"%s, container (major malfunction)!",OPEP);
	phase=999;
	}
      break;
    case 2	:	//take note of this last binding request
      last[count]=curtime;
      count++;
      (void) log_alert(3,"%s, set container binding (isok='%d' count='%d')!",
			  OPEP,isok,count);
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	TRYGAP
#undef	MXTRY
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to check if critical setup    */
/*      is still OK within container.           */
/*						*/
/************************************************/
static _Bool do_check_mounts(const DEVTYP *specdevs)

{
#define OPEP    PRG":do_check_mounts"
#define LPATH   512

static char src[dev_unknown][LPATH];
static char dst[dev_unknown][LPATH];

_Bool isok;

isok=true;
for (int i=0;specdevs[i].str!=(const char *)0;i++) {
  register int ndx;

  ndx=(int)(specdevs[i].devenum);
  switch (specdevs[i].devenum) {
    case dev_cpuinfo          :
    case dev_loadavg          :
    case dev_meminfo          :
    case dev_swaps            :
      if (src[ndx][0]=='\000') {
        (void) snprintf(src[i],LPATH,"/%s/%s","dev",specdevs[ndx].str);
        (void) snprintf(dst[i],LPATH,"/%s/%s","proc",specdevs[ndx].str);
        }
      if (sys_check_remount(src[ndx],dst[ndx])==true)
        isok&=check_binding(); 
      break;
    case dev_lastpid          :
    case dev_acpi             :
    case dev_bus              :
    case dev_interrupts       :
    case dev_ioports          :
    case dev_kcore            :
    case dev_mdstat           :
    case dev_modules          :
    case dev_partitions       :
    case dev_trigger          :
      if (src[ndx][0]=='\000') {
        (void) snprintf(src[i],LPATH,"/%s/%s","dev","null");
        (void) snprintf(dst[i],LPATH,"/%s/%s","proc",specdevs[ndx].str);
        }
      if (sys_check_remount(src[ndx],dst[ndx])==true)
        isok&=check_binding(); 
      break;
    default           :
      (void) log_alert(0,"%s, Unexpected devenum='%d' (Bug?)",
                          OPEP,specdevs[i].devenum);
      isok=false;
      break;
    }
  }
return isok;

#undef  LPATH
#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to update last pid information*/
/*      is still OK within container.           */
/*						*/
/************************************************/
static _Bool do_check_pids()

{
#define OPEP    PRG":do_check_pids"
//where to find kernel last assigned pid
#define LASTPID "/proc/sys/kernel/ns_last_pid"
//Where to set set the lastpid
#define DEVPID  "/dev/lastpid"


static  char prv_val[16]="";

_Bool isok;
char cur_val[sizeof(prv_val)];
int fd;
int n;
int phase;
_Bool proceed;

isok=true;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //Lets open the pids file
      if ((fd=open(LASTPID,O_RDONLY))<0) {
        (void) log_alert(0,"%s, Unable to open <%s> (error=<%s>)",
                          OPEP,LASTPID,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 1      :       //reading file content
      n=pread(fd,cur_val,sizeof(cur_val)-1,0);
      (void) close(fd);
      if (n<=0) {
        (void) log_alert(0,"%s, Unable to read <%s> (error=<%s>)",
                          OPEP,LASTPID,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 2      :       //checking message
      cur_val[n]='\000';
      if (strcmp(cur_val,prv_val)==0)
        phase=999;      // no new pid, nothing to do
      break;
    case 3      :       //opening device
      if ((fd=open(DEVPID,O_RDWR|O_CREAT|O_TRUNC,0600))<0) {
        (void) log_alert(0,"%s, Unable to open <%s> (error=<%s>)",
                          OPEP,DEVPID,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 4      :       //storing new data, everything fine
      if (pwrite(fd,cur_val,n,0)==n) 
        (void) strcpy(prv_val,cur_val);
      (void) close(fd);
      break;
    TOOBAD      :
      isok=false;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef  DEVPID
#undef  LASTPID
#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to done WITHIN the container  */
/*      PID namespace and checking for dynamic  */
/*      data (lastpid, cpuinfo, etc.) to be     */
/*      always accurate and on line.            */
/*	exit.					*/
/*	This must follow the namespace rule	*/
/*						*/
/************************************************/
PUBLIC _Bool mon_monitoring()

{
#define OPEP    PRG":mon_monitoring"
#define TIME_MS 2000            // 2 secondes
//Where to find all mount information for the process
#define MONTINF "/proc/self/mountinfo"

u_long cptevents;
const DEVTYP *specdevs;
struct sigaction sa;
int mnt_inf;
struct pollfd fds[1];
int phase;
_Bool proceed;

verbose=false;
foreground=true;
keep_running=true;
cptevents=0;
(void) memset(&sa,'\000',sizeof(sa));
(void) log_alert(1,"%s, Starting monitoring as PID='%d'",OPEP,getpid());
sa.sa_handler = handle_shutdown;
(void) sigaction(SIGTERM,&sa,NULL);
specdevs=apl_get_specdevs();
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d'",OPEP,phase);
  switch (phase) {
    case 0      :       //EMPTY
      break;
    case 1      :       //EMPTY
      break;
    case 2      :       //initial checkdevs
      keep_running=do_check_mounts(specdevs);
      break;
    case 3      :       //opening /proc/self/mountinfo
      if ((mnt_inf=open(MONTINF,O_RDONLY|O_CLOEXEC))<0) {
        (void) log_alert(0,"%s Unable to open <%s> (error=<%s>",
			    OPEP,MONTINF,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 4      :       //Preparing pool
      fds[0].fd=mnt_inf;        //checking mountinfo
      fds[0].events=POLLPRI;
      break;
    case 5      :       //looping on events
      while (keep_running==true) {
        int ret;

        ret=poll(fds,1,TIME_MS);
        switch (ret) {
          case -1       :       //trouble within pool!
            (void) log_alert(0,"%s poll malfunction! (error=<%s>",
			        OPEP,strerror(errno));
            keep_running=false;
            break;
          case 0        :       //Standard time out, nothing to do
            keep_running=do_check_pids();
            break;
          default       :
            cptevents++;
            if (cptevents>3) 
              (void) log_alert(0,"%s Event detected (checking mount!)",OPEP);
            if ((keep_running=do_check_mounts(specdevs))==true)
            if (fds[0].revents&POLLPRI) {
              //resetting mountinfo change detection
              (void) lseek(mnt_inf,0,SEEK_SET);
              }
            (void) sleep(1);  //To avoid event avalanche
            break;
          }
        }
      break;
    case 6      :       //closing descriptor
      (void) close(mnt_inf);
      break;
    TOOBAD      :
    default     :       //SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
(void) log_alert(0,"%s exiting keep_running='%d'",OPEP,keep_running);
return keep_running;

#undef  MONTINF
#undef  TIME_MS
#undef  OPEP
}

