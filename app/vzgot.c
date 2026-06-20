// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*      Copyright:				*/
/*	 Jean-Marc Pigeon <jmp@safe.ca>	 2009	*/
/*						*/
/************************************************/
/* This program is free software; you can 	*/
/* redistribute it and/or modify it under the 	*/
/* terms of the GNU General Public License as	*/
/* published by the Free Software Foundation	*/
/* version 2 of the License			*/
/*						*/
/* This program is distributed in the hope that */
/* it will be useful, but WITHOUT ANY WARRANTY; */
/* without even the implied warranty of		*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR	*/
/* PURPOSE.  See the GNU General Public License	*/
/* for more details.				*/
/*						*/
/* You should have received a copy of the GNU	*/
/* General Public License along with this 	*/
/* program; if not, write to the Free Software	*/
/* Foundation, Inc., 51 Franklin Street,	*/
/* Fifth Floor, Boston, MA  02110-1301, USA.	*/
/************************************************/
/*						*/
/*	VZBOOT					*/
/*	Its purpose its to boot container 	*/
/*	process and wait for process completion	*/
/*	or a SIGTERM signal, to initiate the	*/
/*	container shutdown.			*/
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
#include	"utlapp.h"
#include	"utlprc.h"
#include	"utlsys.h"
#include	"utlvec.h"
#include	"subcfg.h"
#include	"unicfg.h"
#include	"unilck.h"
#include	"unicnt.h"

#define	APPNAME	"vzgot"
#define	VZOPEN	"vzgot.open"	/*preparing VZ	*/
#define	VZCLOSE	"vzgot.close"	/*stopping VZ	*/
#define	VZSTART	"vzgot.start"	/*container init*/
#define	VZFBOOT	"vzgot.fboot"	/*container boot*/
#define	VZFREE	"vzgot.freeze"	/*  #     freeze*/
#define	VZRBOOT	"vzgot.reboot"	/*  # 	  reboot*/
#define	VZNEW	"vzgot.create"	/*new container	*/
#define	VZDEST	"vzgot.destroy"	/*undo container*/
#define	VZEXEC	"vzgot.exec"	/*in container	*/
#define	VZLIST	"vzgot.list"	/*container list*/
#define	VZMOVE	"vzgot.movefrom"/*container move*/
#define	VZUP	"vzgot.online"	/*container Up	*/
#define	VZSTAND	"vzgot.standby"	/*  # 	backup	*/
#define	VZRBLD	"vzgot.rebuild"	/*redo container*/
#define VZSTAT  "vzstat"        //display container status

#define	FBOOT	"firstboot"	/*firstboot rqst*/

#define CLONEUID (uid_t)0       //clone root UID
#define CLONEGID (gid_t)0       //clone root GID
                              
/*argument structure				*/
typedef	struct	{
	pid_t mspid;	        //vzgot master PID
        int cloneflgs;          //cloning flags
	char *arch;	        //container architecture
	int argc;	        //number of argument
	char **argv;	        //argument list
        const char **targets;   //vproc targets
	}ARGTYP;

char *confdir=(char *)0;        //configuration directory

/*

*/
/************************************************/
/*						*/
/*	Procedure to unshare clone process	*/
/*						*/
/************************************************/
static int vzunshare(int cloneflags)

{
#define	OPEP	"vzgot.c:vzunshare,"
int status;
long uptime;
int phase;
int proceed;

status=-1;
uptime=0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	/*unsharing capability	*/
      if (unshare(cloneflags)<0) {
	(void) fprintf(stdout,"%s, unable unshare capabilities (error=<%s>)\n",
			  	 OPEP,strerror(errno));
	phase=999;	/*abort cloning		*/
        }
      break;
    case 1	:	/*getting uptime	*/
      if (uptime==0) {	/*always		*/
	struct sysinfo s_info;

	(void) memset(&s_info,'\000',sizeof(s_info));
        if (sysinfo(&s_info)<0) {
	  (void) fprintf(stdout,"%s, unable too get HOST boot time (error=<%s>)\n",
			  	 OPEP,strerror(errno));
	  phase=999;	/*abort cloning		*/
          }
	uptime=s_info.uptime-1;
	}
      break;
    case 2	:	/*adjusting boottime	*/
      if ((cloneflags&CLONE_NEWTIME)!=0) {
	char buff[200];
	int fd;
	int len;

	len=snprintf(buff,sizeof(buff),"%d %ld 0",CLOCK_BOOTTIME,-uptime);
	if ((fd=open("/proc/self/timens_offsets", O_WRONLY))<0) {
	  (void) fprintf(stdout,"%s, unable to open times_offsets (error=<%s>)\n",
			  	 OPEP,strerror(errno));
	  phase=999;	/*abort cloning		*/
	  break;
          }
	if (write(fd,buff,len)!=len) {
	  (void) fprintf(stdout,"%s, unable to write <%s> to times_offsets"
			 	" (error=<%s>)\n",
			  	 OPEP,buff,strerror(errno));
	  phase=999;	/*abort cloning		*/
	  break;
	  }
	}
      break;
    case 3	:	/*everything fine	*/
      status=0;
      break;
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
return status;
#undef	OPEP
}
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
		      "[-f] "
		      "[-h] "
		      "[-p] "
		      "[-v] "
		      "action_word "
		      "name [starter]\n",appname);
(void) fprintf(stderr,"\t\t-c confdir   : Use alternative configuration directory (default: \"%s\")\n",
                                          apl_dfltconfdir());
(void) fprintf(stderr,"\t\t-d debug     : Set debug logging level\n");
(void) fprintf(stderr,"\t\t-f           : Foreground mode\n");
(void) fprintf(stderr,"\t\t-h           : Display this help message and exit\n");
(void) fprintf(stderr,"\t\t-p           : Container started in privileged mode\n");
(void) fprintf(stderr,"\t\t-v           : Enable verbose debugging output\n");
(void) fprintf(stderr,"\t\taction_word  : \n");
(void) fprintf(stderr,"\t\t             : boot: Boot container\n");
(void) fprintf(stderr,"\t\t             : reboot: Reboot container\n");
(void) fprintf(stderr,"\t\t             : shutdown: Gracefully shutdown container\n");
(void) fprintf(stderr,"\t\t             : create: Create container from scratch\n");
(void) fprintf(stderr,"\t\t             : destroy: Destroy container filesystem\n");
(void) fprintf(stderr,"\t\t             : enter: Reach an interactive shell inside container\n");
(void) fprintf(stderr,"\t\t             : exec: Execute a command inside container\n");
(void) fprintf(stderr,"\t\t             : freeze: Extract container critical configuration\n");
(void) fprintf(stderr,"\t\t             : movefrom: Move a running container from a remote Host and start it on the local Host\n");
(void) fprintf(stderr,"\t\t             : online: List all online containers uptime\n");
(void) fprintf(stderr,"\t\t             : status: Display the status of a specific container\n");
(void) fprintf(stderr,"\t\tname         : Target container name\n");
(void) fprintf(stderr,"\t\t [starter]   : Program to execute on boot (default: /bin/init)\n");
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to display the firstboot usage*/
/*						*/
/************************************************/
static void fbusage()

{
(void) fprintf(stderr,"%s: %s Version %s\n",appname,FBOOT,apl_getvers());
(void) fprintf(stderr,"usage:  %s "
		      "[-D distname] "
		      "[-d domain] "
		      "[-i IPNUM,[IPALIAS,...]] "
		      "[-h ] "
		      "[-n nodename] "
		      "container_name [starter]\n",FBOOT);
(void) fprintf(stderr,"\t\t-l distname: container linux distribution\n");
(void) fprintf(stderr,"\t\t           : (default RedHat)\n");
(void) fprintf(stderr,"\t\t-d domain  : container domainmame\n");
(void) fprintf(stderr,"\t\t-h         : display firstboot usage\n");
(void) fprintf(stderr,"\t\t-b IPNUM   : container ipnum\n");
(void) fprintf(stderr,"\t\t-n nodename: container node name\n");
(void) fprintf(stderr,"container_name : container name to initialize\n");
(void) fprintf(stderr,"[starter]      : program to start\n");
(void) fprintf(stderr,"\t\t           : (default=\"/bin/init\")\n");
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to set the system architecture*/
/*						*/
/************************************************/
static int setarch(char *arch)

{
static	struct	{
		const char *arch;
		u_long persona;
		}pers[]={
			{"i386",PER_LINUX32_3GB},
			{"i686",PER_LINUX32_3GB},
			{"x86_64",PER_LINUX},
			{(const char *)0,PER_LINUX}
			};
int status;
int i;

status=-1;
errno=EINVAL;
for (i=0;pers[i].arch!=(const char *)0;i++) {
  if (strcmp(pers[i].arch,arch)==0) {
    status=personality(pers[i].persona);
    break;
    }
  }
return status;
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to boot a container		*/
/*						*/
/************************************************/
static int doboot(void *args)

{
#define	OPEP	        "vzgot.c:doboot"
#define	SBININIT	"/sbin/init"

static int todrop[]={	/*capability to drop	*/
	CAP_SYS_RAWIO,	//No access to hardware
        CAP_SYS_MODULE, //Deny to prevent runtime kernel modification 
//	CAP_SYS_BOOT,
//	CAP_MKNOD,      //deny mknod usage within container
	CAP_SYS_TIME    //deny changing Host clock
	};

ARGTYP *parms;
int status;
int phase;
int proceed;

/*SIGCONT received				*/
/*FIRST give time to master to send ITS SIGSTOP	*/
parms=(ARGTYP *)args;
status=0;
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG Phase='%d' contname=<%s>",OPEP,phase,parms->argv[0]);
  status=phase+1;
  switch (phase) {
    case 0	:	//lets go under apparmore umbrella
      break;
    case 1	:	/*lets protect host	*/
      for (int i=0;i<sizeof(todrop)/sizeof(int);i++) {
        if (prctl(PR_CAPBSET_DROP,todrop[i],0,0,0)<0) {
	  (void) log_alert(0,"%s, container %s, unable to remove "
			     "capability (todrop[%d]='%d', error=<%s>)",
			      appname,parms->argv[0],i,todrop[i],strerror(errno));
	  phase=999;	/*trouble trouble	*/
	  }
	}
      break;
    case 2	:	//give time to master to do homework
      (void) log_alert(0,"%s, <%s> Waiting for SIGCONT signal",OPEP,parms->argv[0]);
      for (int i=0;i<20;i++) {
        (void) usleep(100000);
        (void) apl_checksig();
        if (sigcont==true)
          break;
        }
      if (sigcont==false) {
        (void) log_alert(0,"%s, <%s> didn't receive SIGCONT signal in due time",
                            OPEP,parms->argv[0]);
        phase=999;      //trouble
        }
      break;
    case 3      :       //make sure we have the right setuid 
      sigcont=false;    //Ready to receive another signal
      (void) log_alert(0,"%s, <%s> started: NOW PID='%d' UID='%d' GID='%d'",
                          OPEP,parms->argv[0],getpid(),getuid(),getgid());
      if (getuid()!=0) {//are we nobody
        if ((setresuid(0,0,0)<0)||(setresgid(0,0,0)<0)) 
          (void) log_alert(0,"%s CRITICAL: Unable to assume identity 0: %s",
                              OPEP,strerror(errno));
        (void) log_alert(0,"%s, Updated Identity <%s>: UID='%d' GID='%d'",
                            OPEP,parms->argv[0],getuid(),getgid());
        }
      break;
    case 4	:	/*doing preliminary init*/
      if (cnt_initscript(VZSTART,"%s",parms->argv[0])==false) {
	(void) log_alert(0,"%s, container %s unable to init itself",
			    appname,parms->argv[0]);
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 5	:	/*doing root pivot	*/
      if (cnt_pivot(parms->argv[0],parms->cloneflgs)==false) {
	(void) log_alert(0,"%s, container %s unable to pivot itself (aborting)",
			    appname,parms->argv[0]);
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 6      :       //empty phase
      break;
    case 7	:	/*adjusting arch	*/
      if (setarch(parms->arch)<0) {
	(void) log_alert(0,"%s, container <%s>, unable to set architecture "
			   "to <%s> (error=<%s>)",
			    appname,parms->argv[0],parms->arch,strerror(errno));
	phase=999;	/*trouble trouble	*/
        }
      break;
   case 8	:
      if (vzunshare(CLONE_NEWTIME)<0) {
	(void) log_alert(0,"%s, container <%s> unable to set boot time (error=<%s>)",
			    appname,parms->argv[0],strerror(errno));
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 9     :
      if (app_load_profile(getenv(APP_PRO))==false) {
        (void) log_alert(0,"%s Unable to load container profile (Abort!)",OPEP);
	phase=999;	/*trouble trouble	*/
        }
      break;
    case 10	:	//detaching interface
      (void) sys_detach_from_supervisor(parms->argv[0]);
      break;
    case 11	:	/*starting master jobs	*/
      parms->argc--;
      parms->argv++;
      (void) closelog();
      if (parms->argc==0)	/*daemon start	*/
        (void) execl(SBININIT,SBININIT,(char *)0);
      else
        (void) execv(parms->argv[0],parms->argv);
      (void) openlog(parms->argv[0],LOG_NDELAY|LOG_PID,LOG_DAEMON);
      (void) log_alert(0,"%s, container %s unable to start exec (errro=<%s>)",
			 appname,parms->argv[-1],strerror(errno));
      break;
    default	:	/*SAFE guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
(void) sleep(1);	/*lets relax			*/
(void) exit(status);

#undef  SBININIT
#undef	OPEP
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to execute a container shell.	*/
/*						*/
/************************************************/
static int vzexec(int argc,char *argv[])

{
#define FMT     "vzexec is unable to find main pid for container <%s>\n"

int status;
int cntpid;
int phase;
int proceed;

status=0;
cntpid=-1;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	/*container name?	*/
      if ((argv[0]==(char *)0)||(strlen(argv[0])==0)) {
	(void) log_alert(0,"No container name specified!");
	status=1;
	proceed=false;	/*trouble trouble	*/
	}
      break;
    case 1	:	/*load container config	*/
      if (cfg_loadconfig(confdir,argv[0])!=0) {
	proceed=false;	/*trouble trouble	*/
        }
      break;
    case 2	:	/*getting container pid	*/
      if ((cntpid=cnt_getclonepid(argv[0]))==(pid_t)0) {
	(void) fprintf(stderr,FMT,argv[0]);
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 3	:	/*calling initscript	*/
      if (cntpid!=0) {	/*always		*/
	char *params;
	int i;

	params=strdup("");
	for (i=1;i<argc;i++) {
	  params=realloc(params,strlen(params)+strlen(argv[i])+2);
	  (void) strcat(params,argv[i]);
	  (void) strcat(params," ");
	  }
        if ((status=cnt_injectcmd(VZEXEC,argv[0],cntpid,params))!=0) {
	  (void) log_alert(0,"Unable to exec command within "
			     "container <%s> (status=%d)",
			     argv[0],status);
	  status=2;
	  phase=999;	/*trouble trouble	*/
	  }
	(void) free(params);
        }
      break;
    default	:	/*SAFE guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
return status;

#undef  FMT
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to execute a container status */
/*      report.                                 */
/*						*/
/************************************************/
static int vzstatus(int argc,char *argv[])

{
#define	OPEP	        "vzgot.c:vzstatus"
int status;
char *libexecpath;
int phase;
int proceed;

status=0;
libexecpath=apl_appdir(d_libexec);
phase=0;
proceed=true;
while (proceed==true) {
  status++;             //incrementing the failur status number
  switch (phase) {
    case 0	:	/*container name?	*/
      if ((argv[0]==(char *)0)||(strlen(argv[0])==0)) {
	(void) fprintf(stdout,"No container name specified!\n");
        phase=999;      //no need to go further
	}
      break;
    case 1	:	//executing vzstat
      if (argc>0) {
        char **newargs;
        char execpath[200];

        (void) snprintf(execpath,sizeof(execpath),"%s/%s",libexecpath,VZSTAT);
        newargs=(char **)vec_addlstlst((void **)0,(void *)strdup(execpath));
        for (int i=0;i<argc;i++)
          newargs=(char **)vec_addlstlst((void **)newargs,(void *)strdup(argv[i]));
        if (execv(newargs[0],newargs)<0) {
          (void) fprintf(stdout,"%s, unable initiate status request (errro=<%s>)\n",
			         appname,strerror(errno));
          phase=999;    //trouble trouble
          }
        newargs=(char **)vec_freelstlst((void **)newargs,VECFREE);
        }
      break;
    default	:	/*SAFE guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
libexecpath=apl_freestr(libexecpath);
return status;

#undef  OPEP
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to execute a specific shell	*/
/*	script.					*/
/*						*/
/************************************************/
static int vzscript(const char *script,int argc,char *argv[])

{
int status;
int phase;
int proceed;

status=0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	/*container name?	*/
      if (status==0) { 	/*always		*/
	char *params;
	int i;

	params=strdup("");
	for (i=0;i<argc;i++) {
	  params=realloc(params,strlen(params)+strlen(argv[i])+2);
	  (void) strcat(params,argv[i]);
	  (void) strcat(params," ");
	  }
        if (cnt_initscript(script,"%s",params)==false) {
	  (void) log_alert(0,"Unable to execute \'%s\' command",script);
	  status=1;
	  phase=999;	/*trouble trouble	*/
	  }
	(void) free(params);
        }
      break;
    default	:	/*SAFE guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
return status;
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to stop a container		*/
/*						*/
/************************************************/
static int cont_shutdown(int argc,char *argv[])

{
#define OPEP    "vzgot.c:cont_shutdown"

int status;
pid_t cntpid;
int phase;
int proceed;

status=1;
cntpid=(pid_t)0;
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d'",OPEP,phase);
  switch (phase) {
    case 0	:	/*load container config	*/
      if (cfg_loadconfig(confdir,argv[0])!=0) {
	(void) fprintf(stderr,"Shutdown is unable to find container <%s> "
			      "config file\n",argv[0]);
	proceed=false;	/*trouble trouble	*/
        }
      break;
    case 1	:	/*getting container pid	*/
      if ((cntpid=cnt_getclonepid(argv[0]))==(pid_t)0) {
	(void) fprintf(stderr,"Shutdown is unable to find container <%s> "
			      "main pid\n",argv[0]);
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 2	:	/*sending SIGINT	*/
      if (cntpid!=(pid_t)0) {	/*always	*/
	char *parms[]={argv[0],"shutdown -h -H now",(char *)0};

	if ((status=vzexec(2,parms))!=0) {
	  (void) fprintf(stderr,"Shutdown is unable to powerdown container "
			      "<%s> main pid (error='%d')\n",argv[0],status);
          (void) cnt_rmclonepid(argv[0]);
	  phase=999;	/*trouble trouble	*/
	  }
	}
      break;
    case 3	:	/*waiting a little bit	*/
      (void) log_alert(0,"Shutting down container <%s> with pid='%03d'",
		          argv[0],cntpid);

      //(void) sys_show_namespace();JMPDBG
      (void) sleep(1);	/*container stop	*/
      break;
    case 4	:	/*waiting full stop	*/
      if (cntpid!=(pid_t)1) {	/*always	*/
	char cmd[200];
	int i;

	(void) snprintf(cmd,sizeof(cmd),"ps --ppid %d  | wc -l",cntpid);
	for (i=0;i<15;i++) {
	  FILE *fichier;
	  int remaining;

	  remaining=0;
	  if ((fichier=popen(cmd,"r"))==(FILE *)0) {
	    (void) log_alert(0,"%s, shutdown pipe <%s> unable to proceed "
                               "(error=<%s>)",
				appname,cmd,strerror(errno));
	    break;
	    }
	  if (fscanf(fichier,"%d",&remaining)!=1) 
	    (void) log_alert(0,"%s, shutdown pipe <%s> not able to catch answer)",appname,cmd);
	  (void) pclose(fichier);
	  if (remaining<2) {
	    status=0;
	    break;	/*only remaining line "PID TTY TIME CMD"	*/
	    }
	  (void) sleep(1);
	  }
	/*Terminating container main process	*/
  	(void) kill(cntpid,SIGKILL);
	}
      break;
    default	:	/*SAFE guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
return status;

#undef  OPEP
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to create a brand new		*/
/*	container.				*/
/*						*/
/************************************************/
static int create(int argc,char *argv[])

{
#define	NCREATE	1

int status;
int phase;
int proceed;
char format[20];

status=0;
(void) strcpy(format,"%s");
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	/*check parameters num	*/
      if (argc<NCREATE) {
	(void) log_alert(0,"Found %d parameter expecting %d",argc,NCREATE);
	status=1;
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 1	:	/*calling initscript	*/
      if (argv[1]!=(char *)0) 
        (void) strcpy(format,"%s %s");
      if (cnt_initscript(VZNEW,format,argv[0],argv[1])==false) {
	(void) log_alert(0,"Unable to create container <%s>",argv[0]);
	status=2;
	phase=999;	/*trouble trouble	*/
        }
      break;
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
return status;
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to destroy a container	*/
/*	structure.				*/
/*						*/
/************************************************/
static int destroy(int argc,char *argv[])

{
#define	NDEST	1

int status;
int phase;
int proceed;

status=0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	/*check parameters num	*/
      if (argc<NDEST) {
	(void) log_alert(0,"Found %d parameter expecting %d",argc,NDEST);
	status=1;
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 1	:	/*calling initscript	*/
      if (cnt_initscript(VZDEST,"%s",argv[0])==false) {
	(void) log_alert(0,"Unable to destroy container <%s>",argv[0]);
	phase=999;	/*trouble trouble	*/
        }
      break;
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
return status;
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to start a container.		*/
/*						*/
/************************************************/
static int boot(int argc,char *argv[])

{
#define	OPEP	        "vzgot.c:boot"
#define STACK_SIZE      (1024 * 1024)   //clone own stack

//defining all proc info data we wait accessible
//to container
static  const char *targets[]={
        "meminfo",
        (char *)0
        };

int status;
int cloneflgs;
char *stack;
char *cntdist;
char *cntarch;
int consolefd;
pid_t cpid;
int phase;
int proceed;

status=0;
cloneflgs=0;
cloneflgs|=SIGCHLD;
//cloneflgs|=__WALL;
//cloneflgs|=__WCLONE;
cloneflgs|=CLONE_NEWNET;
cloneflgs|=CLONE_NEWIPC;
cloneflgs|=CLONE_NEWNS;
cloneflgs|=CLONE_NEWPID;
cloneflgs|=CLONE_NEWUTS;
cloneflgs|=CLONE_NEWCGROUP;
cloneflgs|=CLONE_NEWTIME;
stack=mmap(NULL,STACK_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK,-1,0);
cntdist=(char *)0;
cntarch=(char *)0;
cpid=(pid_t)0;
consolefd=-1;
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG ->phase='%2d'",OPEP,phase);
  switch (phase) {
    case  0	:	/*container name?	*/
      if ((argv[0]==(char *)0)||(strlen(argv[0])==0)) {
	(void) log_alert(0,"No container name specified!");
	status=1;
	proceed=false;	/*trouble trouble	*/
	}
      break;
    case  1	:	/*load container config	*/
      if (cfg_loadconfig(confdir,argv[0])!=0) {
	status=phase;
	proceed=false;	/*trouble trouble	*/
        }
      break;
    case  2	:	//loadin apparmor profile
      if (app_parse_profile(getenv(APP_PRO),true)==false) {
	(void) log_alert(0,"Unable to load container <%s> apparmor profile (%s!)",
                            argv[0],"abort");
	status=phase;
	proceed=false;	/*trouble trouble	*/
        }
      break;
    case  3	:	//check configuration privileged mode
      if (privileged==false) {
        char *ptr;

        if ((ptr=getenv(PRIVILEGED))!=(char *)0) {
          if (strcasecmp(ptr,"YES")==0)
            privileged=true;
          }
        }
      break;
    case  4	:	/*getting a lock	*/
      cntarch=cnt_getarch(argv[0]);
      cntdist=cnt_getdist(argv[0]);
      (void) prc_divedivedive(foreground,appname);
      (void) cnt_setstdio(foreground,argv[0],"master.log");
      (void) prc_settitle("%s, starting %s container",appname,argv[0]);
      if (lck_locking(argv[0],LCK_LOCK,5)==false) {
	(void) log_alert(0,"Unable to lock container <%s> access",argv[0]);
	status=phase;
	proceed=false;	/*trouble trouble	*/
	}
      break;
    case  5      :      //set supervisor process on launch pad
      if (cnt_launch(argv[0])==false) {
        (void) log_alert(0,"Unable to start container's <%s> supervisor (config?)",
                           argv[0]);
        status=phase;
        proceed=false;  /*trouble trouble       */
        }
      break;
    case  6     :       //creating info directory
      if (cnt_create_infos(true)==false) {
        (void) log_alert(0,"Unable to mount container <%s> infos area (config?)",
                           argv[0]);
        status=phase;
        proceed=false;  /*trouble trouble       */
        }
      break;		
    RESTART     :       //NO BREAK
    case  7	:	/*(re)start proceeding	*/
      phase=7;          //this is a start/restart
      if (argv[0]!=(char *)0) {	/*always	*/
	static ARGTYP args;
        int locerr;
	
	args.mspid=getpid();
	args.arch=cntarch;
        if (privileged==false)  //set the standard working mode
          cloneflgs|=CLONE_NEWUSER; 
        args.cloneflgs=cloneflgs;
        args.targets=targets;
	args.argc=argc;
	args.argv=argv;
        (void) closelog();
      	cpid=clone(&doboot,stack+STACK_SIZE,cloneflgs,(void *)&args);
        locerr=errno;
        (void) openlog(appname,LOG_NDELAY|LOG_PID,LOG_DAEMON);
        if (cpid<0) {
	  (void) log_alert(0,"%s, Unable to clone %s container "
			     "process (error=<%s>)",
			     appname,argv[0],strerror(locerr));
	  status=phase;
	  phase=999;	/*trouble trouble	*/
	  }
	}
      break;
    case  8	:	//Master process set map user IF CLONE_NEWUSER set
      (void) kill(cpid,SIGSTOP);        //stopping container first
      (void) log_alert(0,"Started container <%s> with pid='%04d'",argv[0],cpid);
      if (privileged==true) {
        (void) log_alert(0,"%s, Caution! Container <%s> started in <%s> mode",
                            OPEP,argv[0],PRIVILEGED);
        phase++;        //Privileged mode!, No need to maps UID
        }
      break;
    case  9	:	//Master process set map user IF CLONE_NEWUSER set
      if (cnt_mapcontids(cpid,CLONEUID,CLONEGID)==false) {
        (void) log_alert(0,"%s, Unable to set user map (CLONE_NEWUSER missing?) "
                           "Abort!",OPEP);
	phase=999;	/*trouble trouble	*/
        }
      break;
    case 10	:	//Opening the container FIFO console
      if ((consolefd=cnt_open_cont_console(argv[0]))<0) {
        (void) log_alert(0,"%s, Unable to open container console FIFO (Abort!)",
                           OPEP);
	phase=999;	/*trouble trouble	*/
        }
      break;
    case  11	:	/*call to init script	*/	
      //(void) log_alert(0,"%s VZOPEN=<%s>,argv[0]=<%s>","JMPDBG",VZOPEN,argv[0]);
      if (cnt_initscript(VZOPEN,"%s %d %d",argv[0],getpid(),cpid)==false) {
	(void) log_alert(0,"%s, Init script failure aborting %s "
			   "container starting process",
			   appname,argv[0]);
	(void) kill(cpid,SIGKILL);
	status=phase;
	phase=999;	/*trouble trouble	*/
	}
      break;
    case  12	:	/*setting clone PID	*/
      if (cnt_setclonepid(argv[0],cpid)==false) {
	(void) kill(cpid,SIGKILL);
	status=phase;
	phase=999;	/*trouble trouble	*/
	}
      break;
    case  13	:	//pining name space
      if (sys_pin_cgroup_ns(cpid)==false) {
	(void) kill(cpid,SIGKILL);
	status=phase;
	phase=999;	/*trouble trouble	*/
        }
      break;
    case  14	:	//set container limit   
      if (cnt_setcgroup(argv[0])==false) {
	(void) log_alert(0,"%s, Unable to set container <%s> working limits (%s)",
			    appname,argv[0],"config?");
	status=phase;
	phase=999;	/*trouble trouble Abort	*/
        }
      break;
    case  15	:	//setting the starting time
      if (sys_set_start(cpid)==false) {
	(void) log_alert(0,"%s, Unable to set container <%s> starting time (%s)",
			    appname,argv[0],"system?");
	status=phase;
	phase=999;	/*trouble trouble Abort	*/
        }
      break;
    case  16	:	/*waking up container 	*/
      if (kill(cpid,SIGCONT)<0) {
	(void) log_alert(0,"%s, Unable to wakeup container %s process (error=<%s>)",
			    appname,argv[0],strerror(errno));
	(void) kill(cpid,SIGKILL);
	status=phase;
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 17	:	/*waiting process	*/
      (void) prc_settitle("%s: container %s (%s/%s) up",
		      	  appname,argv[0],cntdist,cntarch);
      for (int i=0;i<20;i++) {
        (void) usleep(100000);
        if (apl_checksig()==0)
          break;        //this is a real timeout
        }
      (void) sleep(3);
      (void) log_alert(0,"%s, Starting cnt_mstconsole",OPEP);
      status=cnt_mstconsole(argv[0],cpid,consolefd);
      break;
    case 18	:	/*restart neeeded status*/
      if (status==1) {
	(void) log_alert(2,"Rebooting <%s> container",argv[0]);
        (void) prc_settitle("%s: container %s is rebooting",appname,argv[0]);
        (void) close(consolefd); 
	(void) sleep(2);
	goto RESTART;	//rebooting sequence... 
	}
      break;
    case 19	:	//excuting close shell script
      (void) prc_settitle("%s: container %s in shutdown process",appname,argv[0]);
      if (cnt_initscript(VZCLOSE,"%s %d",argv[0],cpid)==false) {
	(void) log_alert(0,"%s, closing script failure for %s container "
			   "stopping",
			   appname,argv[0]);
	status=phase;
	}
      (void) cnt_rmclonepid(argv[0]);
      if (lck_locking(argv[0],LCK_UNLOCK,1)==false) {
	(void) log_alert(0,"Unable to unlock container <%s> acces",argv[0]);
	status=phase;
	}
      break;
    case 20	:	//making sure we exit properly
      if (cnt_exit(cpid,argv[0])==false) {
        status=phase;
        phase=999;
        }
      break;
    default	:	/*SAFE guard		*/
      (void) close(consolefd);
      cntdist=apl_freestr(cntdist);
      cntarch=apl_freestr(cntarch);
      proceed=false;
      break;
    }
  phase++;
  }
(void) munmap(stack,STACK_SIZE);
return status;

#undef	OPEP
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to restart a container.	*/
/*						*/
/************************************************/
static int doreboot(int argc,char *argv[])

{
int status;
int phase;
int proceed;

status=0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	/*container shutdown	*/
      if ((status=cont_shutdown(argc,argv))!=0) {
	(void) fprintf(stderr,"reboot is unable to shutdown container <%s>\n",argv[0]);
	}
      break;
    case 1	:	/*wait for settle down	*/
      (void) sleep(1);	/*one second relaxe	*/
      break;
    case 2	:	/*boot container	*/
      if ((status=boot(argc,argv))!=0) {
	(void) fprintf(stderr,"reboot is unable to boot container <%s>\n",argv[0]);
	phase=999;	/*trouble trouble	*/
	}
      break;
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
return status;
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to start a container.		*/
/*						*/
/************************************************/
static int firstboot(int argc,char *argv[])

{
#define	DFLDIST	"RedHat"/*default distribution	*/

int status;
char *distrib;
char *domain;
char *ipnums;
char *nodename;
int phase;
int proceed;
char c;

status=0;
distrib=strdup(DFLDIST);
domain=(char *)0;
ipnums=(char *)0;
nodename=(char *)0;
phase=0;
proceed=true;
optind=1;
while ((c=getopt(argc,argv,"+D:d:hi:l:n:"))!=EOF) {
  switch(c) {
    case   'D'  :
      if (distrib!=(char *)0)
	(void) free(distrib);
      distrib=strdup(optarg);
      break;
    case   'd'  :
      if (domain!=(char *)0)
	(void) free(domain);
      domain=strdup(optarg);
      break;
    case   'i'  :
      if (ipnums!=(char *)0)
	(void) free(ipnums);
      ipnums=strdup(optarg);
      break;
    case   'n'  :
      if (nodename!=(char *)0)
	(void) free(nodename);
      nodename=strdup(optarg);
      break;
    case   'h'  :
    default	:
      (void) fbusage();
      break;
    }
  }
argc+=optind;
argv+=optind;
while (proceed==true) {
  switch (phase) {
    case 0	:	/*container name	*/
      if ((argv[0]==(char *)0)||(strlen(argv[0])==0)) {
	(void) log_alert(0,"No container name specified!");
	status=1;
	proceed=false;	/*trouble trouble	*/
	}
      break;
    case 1	:	/*checking parameters	*/
      if (nodename==(char *)0)
	nodename=strdup(argv[0]);
      if (domain==(char *)0) {
	struct utsname un;
	char *ptr;

	(void) uname(&un);
	if ((ptr=strchr(un.nodename,'.'))!=(char *)0)
	  ptr++;
	else
	  ptr=un.nodename;
	domain=strdup(ptr);
	}
      if (ipnums==(char *)0)
	ipnums=strdup("");
      break;
    case 2	:	/*firstboot script	*/
      if (cnt_initscript(VZFBOOT,"%s %s %s %s %s",
				 argv[0],distrib,domain,nodename,ipnums)==false) {
	(void) log_alert(0,"%s, container %s unable to proceed "
			   "with %s",
			   appname,argv[0],VZFBOOT);
        (void) sleep(3);
	status=phase;
	phase=999;	/*trouble trouble	*/
	}
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
(void) free(nodename);
(void) free(ipnums);
(void) free(domain);
(void) free(distrib);
return status;
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to switch about proceeding	*/
/*						*/
/************************************************/
static int action(int argc,char *argv[])

{
#define OPEP    "vzgot.c:action"

int status;
char *parms[]={"contname","/bin/sh",(char *)0};

status=0;
if (argc<=0) {
  (void) log_alert(0,"%s action string requested",OPEP);
  (void) usage();
  status=1;
  }
else {
  enum	{
	a_firstboot,	/*container firstboot		*/
	a_boot,		/*booting container		*/
	a_reboot,	/*rebooting container		*/
	a_shutdown,	/*container shutdown		*/
	a_create,	/*creating container		*/
	a_destroy,	/*removing container		*/
	a_enter,	/*entering in container		*/
	a_exec,		/*command in container		*/
	a_freeze,	/*free container contents	*/
	a_list,		/*get container list		*/
	a_movefrom,	/*move container from other host*/
	a_online,	/*get container Up list		*/
	a_standby,	/*container backup		*/
	a_status,	/*container status report	*/
	a_rebuild,	/*rebuild container		*/
	a_unknown	/*unknown action		*/
	};

  static VOCTYP vocaction[]={
	{a_firstboot,FBOOT,(const void *)0},
	{a_boot,	"boot",		(const void *)0},
	{a_list,	"list",		(const void *)0},
	{a_reboot,	"reboot",	(const void *)0},
	{a_exec,	"exec",		(const void *)0},
	{a_enter,	"enter",	(const void *)0},
	{a_freeze,	"freeze",	(const void *)0},
	{a_create,	"create",	(const void *)0},
	{a_online,	"online",	(const void *)0},
	{a_destroy,	"destroy",	(const void *)0},
	{a_standby,	"standby",	(const void *)0},
	{a_movefrom,	"movefrom",	(const void *)0},
	{a_shutdown,	"shutdown",	(const void *)0},
	{a_status,	"status",	(const void *)0},
	{a_rebuild,	"rebuild",	(const void *)0},
	{a_unknown,	(char *)0,	(const void *)0}
	};

  VOCTYP *voc;

  (void) setenv("COLUMNS","80",true);
  if ((voc=apl_getvoca(vocaction,argv[0]))!=(VOCTYP *)0) {
    switch(voc->code) {
      case a_firstboot	:	
        (void) free(argv[0]);
        argv[0]=strdup(voc->key);       //generate memory leak
        status=firstboot(argc,argv);
        break;
      case a_boot	:	
         status=boot(argc-1,argv+1);
        break;
      case a_reboot	:	
         status=doreboot(argc-1,argv+1);
        break;
      case a_shutdown	:	
        status=cont_shutdown(argc-1,argv+1);
        break;
      case a_status	:	
        status=vzstatus(argc-1,argv+1);
        break;
      case a_create	:	
        status=create(argc-1,argv+1);
        break;
      case a_destroy	:	
        status=destroy(argc-1,argv+1);
        break;
      case a_enter	:	
	parms[0]=argv[1];
        status=vzexec(2,parms);
        break;
      case a_exec	:	
        status=vzexec(argc-1,argv+1);
        break;
      case a_freeze	:	
        status=vzscript(VZFREE,argc-1,argv+1);
        break;
      case a_list	:	
        status=vzscript(VZLIST,argc-1,argv+1);
        break;
      case a_movefrom	:	
        status=vzscript(VZMOVE,argc-1,argv+1);
        break;
      case a_online	:	
        status=vzscript(VZUP,argc-1,argv+1);
        break;
      case a_standby	:	
        status=vzscript(VZSTAND,argc-1,argv+1);
        break;
      case a_rebuild	:	
        status=vzscript(VZRBLD,argc-1,argv+1);
        break;
      case a_unknown	:
      default		:
	(void) fprintf(stderr,"<%s> unknown action string\n\n",argv[0]);
	(void) usage();
        status=1;	
        break;
      }
    }
  }
return status;

#undef  OPEP
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
int status;
char c;

appname=APPNAME;
status=0;
(void) apl_trapsegv(true);
(void) openlog(appname,LOG_NDELAY|LOG_PID,LOG_DAEMON);
(void) apl_settrap(true);
argv=prc_preptitle(argc,argv,environ);
while ((c=getopt(argc,argv,"+c:d:Ufhv"))!=EOF) {
  switch(c) {
    case   'c'  :
      confdir=apl_freestr(confdir);
      confdir=strdup(optarg);
      break;
    case   'd'  :
      debug=atoi(optarg);
      break;
    case   'f'  :
      foreground=true;
      break;
    case   'p'  :
      privileged=true;
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
(void) sys_set_random_seed();
if (status==0) {
  int phase;
  int proceed;

  phase=0;
  proceed=true;
  while (proceed==true) {
    switch (phase) {
      case 0	:	/*executing action		*/
        status=action(argc-optind,argv+optind);
	break;
      default	:	/*SAFE Guard			*/
        proceed=false;
	break;
      }
    phase++;
    if (status!=0)
      break;
    }
  }
confdir=apl_freestr(confdir);
argv=prc_cleantitle();
(void) apl_settrap(false);
(void) closelog();
(void) apl_trapsegv(false);
exit(status);
}
