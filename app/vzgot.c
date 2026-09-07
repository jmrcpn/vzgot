// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*      Copyright:				*/
/*	 Jean-Marc Pigeon <jmp@safe.ca>	 2009	*/
/*						*/
/************************************************/
/*						*/
/*	VZGOT					*/
/*	Its purpose its to manage container. 	*/
/*						*/
/************************************************/
#include	<linux/reboot.h>
#include	<linux/sched.h>
#include	<sys/capability.h>
#include	<sys/mount.h>
#include	<sys/mman.h>
#include	<sys/personality.h>
#include	<sys/prctl.h>
#include	<sys/reboot.h>
#include	<sys/syscall.h>
#include	<sys/sysinfo.h>
#include	<sys/utsname.h>
#include	<sys/wait.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<limits.h>
#include	<poll.h>
#include	<sched.h>
#include	<stdio.h>
#include	<string.h>
#include	<syslog.h>
#include	<signal.h>
#include	<stdlib.h>
#include	<sched.h>
#include	<time.h>

#include	"dbglog.h"
#include	"lowapl.h"
#include	"lowtyp.h"
#include	"utlapl.h"
#include	"utlapp.h"
#include	"utlprc.h"
#include	"utlsys.h"
#include	"utlvec.h"
#include	"subprc.h"
#include	"subcfg.h"
#include	"unicfg.h"
#include	"unilck.h"
#include	"unicnt.h"

#define	APPNAME	        "vzgot"
#define PRG             "vgot.c"

#define VZGOT_ROOT      "/vzgot/test"

//to do network up and down
#define VZETHUP         APPNAME".eth.up"
#define VZETHDOWN       APPNAME".eth.down"

//to use clone3
#ifndef SYS_clone3
#define SYS_clone3 435
#endif

//defining privot fonction
#define pivot_root(new_root,put_old) syscall(SYS_pivot_root,new_root,put_old)

#define	VZOPEN	"vzgot.open"	/*preparing VZ	*/
#define	VZCLOSE	"vzgot.close"	/*stopping VZ	*/
#define	VZSTART	"vzgot.start"	/*container init*/
#define	VZBOOT	"vzgot.boot"	/*container boot*/
#define	VZFREE	"vzgot.freeze"	/*  #     freeze*/
#define	VZON	"vzgot.onboot"	/*  #     onboot*/
#define	VZOFF	"vzgot.offboot"	/*  #     onboot*/
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

char *confdir=(char *)0;        //configuration directory

//configuration structure
typedef struct  {
        char contname[120];     //container name
        char contarch[30];      //container working architecture
        char contdist[50];      //container distribution
        int slave_fd;           //sub process monitor
        pid_t logger_pid;       //Sub process to monitor console
        int sync_pipe[2];       //synchronising pipe
	char **exec_args;       //container execve program
        }CONTYP;

/*

*/
/************************************************/
/*						*/
/*	Procedure to display the program usage	*/
/*						*/
/************************************************/
static void usage()

{
(void) fprintf(stderr,"%s: Version %s\n",appname,apl_getvers());
(void) fprintf(stderr,"usage:  %s "
		      "[-c confdir] "
		      "[-d debug] "
		      "[-f] "
		      "[-h] "
		      "[-p] "
		      "[-v] "
		      "[-V] "
		      "action_word "
		      "name [starter]\n",appname);
(void) fprintf(stderr,"\t\t-c confdir   : Use alternative configuration directory (default: \"%s\")\n",
                                          apl_dfltconfdir());
(void) fprintf(stderr,"\t\t-d debug     : Set debug logging level\n");
(void) fprintf(stderr,"\t\t-f           : Foreground mode\n");
(void) fprintf(stderr,"\t\t-h           : Display this help message and exit\n");
(void) fprintf(stderr,"\t\t-p           : Start container in privileged mode\n");
(void) fprintf(stderr,"\t\t-v           : Enable verbose debugging output\n");
(void) fprintf(stderr,"\t\t-V           : Display version number\n");
(void) fprintf(stderr,"\t\taction_word  : \n");
(void) fprintf(stderr,"\t\t               boot: Boot container\n");
(void) fprintf(stderr,"\t\t               onboot: Enable container autostart on Host boot sequence\n");
(void) fprintf(stderr,"\t\t               offboot: Disable container autostart on Host boot sequence\n");
(void) fprintf(stderr,"\t\t               reboot: Restart container\n");
(void) fprintf(stderr,"\t\t               shutdown: Gracefully shut down container\n");
(void) fprintf(stderr,"\t\t               create: Create container from scratch\n");
(void) fprintf(stderr,"\t\t               destroy: Destroy container filesystem\n");
(void) fprintf(stderr,"\t\t               enter: Open an interactive shell inside container\n");
(void) fprintf(stderr,"\t\t               exec: Execute a command inside container\n");
(void) fprintf(stderr,"\t\t               freeze: Extract container critical configuration\n");
(void) fprintf(stderr,"\t\t               movefrom: Move a running container from a remote Host and start it on the local Host\n");
(void) fprintf(stderr,"\t\t               online [-r s]: List online containers uptime (refresh every 's' sec)\n");
(void) fprintf(stderr,"\t\t               status [-r]: container status display (refresh every sec with -r)\n");
(void) fprintf(stderr,"\t\tname         : Target container name\n");
(void) fprintf(stderr,"\t\t[starter]    : Program to execute on boot (default: /bin/init)\n");
}
/*

*/
/************************************************/
/*						*/
/*	pidfd_open is a wrapper around a linux  */
/*      syscall, to  obtain a file descriptor   */
/*      that refers to a process.               */
/*						*/
/************************************************/
PUBLIC int pidfd_open(pid_t pid,unsigned int flags)

{
return syscall(SYS_pidfd_open,pid,flags);
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
#define OPEP    PRG":setarch"

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
    if ((status=personality(pers[i].persona))<0) {
      (void) log_alert(0,"%s Unable to set personality <error <%s>)",
                          OPEP,strerror(errno));
      };
    break;
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
/*	Procedure to do container monitoring    */
/*						*/
/************************************************/
static _Bool monitoring(int mfd)

{
#define OPEP    PRG":monitoring"


static char *argv[]={"[vzgot]",(char *)0};
static char *env[]={(char *)0};

_Bool isok;
int fd;
pid_t tracker;

isok=false;
tracker=fork();
switch (tracker) {
  case -1       :       //troouble trouble
    (void) log_alert(0,"%s, unable to start tracker (error=<%s>)",
                            OPEP,strerror(errno));
    break;
  case 0        :       //the child itself
    if (setsid()<0) {
      (void) exit(EXIT_FAILURE);
      }
    if ((fd=open("/dev/console",O_WRONLY|O_CREAT,0600))>=0) {
      //(void)dup2(fd,STDIN_FILENO);
      (void)dup2(fd,STDOUT_FILENO);
      (void)dup2(fd,STDERR_FILENO);
      //if (fd>STDERR_FILENO)
      (void) close(fd);
      }
    (void) prctl(PR_SET_NAME,"[vzgot]",0,0,0);
    (void) prc_settitle("[vzgot]");
    if (fexecve(mfd,argv,env)<0) {
      (void) log_alert(0,"%s, unable to exec vzmon from memory (error=<%s>)",
                            OPEP,strerror(errno));
      }
    (void) _exit(0);
    break;
  default       :       //supervisor, everything fine
    isok=true;
    break;
  }
return isok;

#undef  OPEP
}
/*
^L
*/
/************************************************/
/*						*/
/*	small procedure to read a file descripor*/
/*      and forward contents to a fi,e.         */
/*						*/
/************************************************/
static void pty_tracker(int master_fd,const char *log_path)

{
#define OPEP PRG":pty_tracker"

int log_fd;
ssize_t n_read;
char buf[512];
int phase;
_Bool proceed;

log_fd=-1;
n_read=(size_t)0;
buf[0]='\000';
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //Opening the file
      if ((log_fd=open(log_path,O_WRONLY| O_CREAT|O_TRUNC,0600))<0) {
        (void) log_alert(0,"%s, unable to open <%s> (error=<%s>)",
                            OPEP,log_path,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 1      :       //tracking the pty and storing data in log_path
      while ((n_read=read(master_fd,buf,sizeof(buf)-1))>0) {
        if (write(log_fd,buf,n_read)<0) {
          buf[n_read]='\000';
          (void) log_alert(0,"%s, unable to write <%s>  to <%s> (error=<%s>)",
                            OPEP,buf,log_path,strerror(errno));
          break;
          }
        }
      close(log_fd);
      break;
    TOOBAD      :
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
#undef  OPEP
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
      if ((cntpid=cnt_get_cont_pid(argv[0]))==(pid_t)0) {
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
/*	procedure to stop a container		*/
/*						*/
/************************************************/
static int cont_shutdown(int argc,char *argv[])

{
#define OPEP    PRG":cont_shutdown"

int status;
pid_t cntpid;
int phase;
int proceed;

status=1;
cntpid=(pid_t)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	/*load container config	*/
      if (cfg_loadconfig(confdir,argv[0])!=0) {
	(void) fprintf(stderr,"Shutdown is unable to find container <%s> "
			      "config file\n",argv[0]);
	proceed=false;	/*trouble trouble	*/
        }
      break;
    case 1	:	/*getting container pid	*/
      if ((cntpid=cnt_get_cont_pid(argv[0]))==(pid_t)0) {
	(void) fprintf(stderr,"Shutdown is unable to find container <%s> "
			      "main pid\n",argv[0]);
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 2	:	/*requesting POWEROFF	*/
      if (cntpid!=(pid_t)0) {	/*always	*/
        char *parms[]={
                        argv[0],
                        "poweroff",
                        (char *)0};

	if ((status=vzexec((sizeof(parms)/sizeof(char *))-1,parms))!=0) {
	  (void) fprintf(stderr,"Shutdown is unable to powerdown container "
			      "<%s> main pid (error='%d')\n",argv[0],status);
          (void) cnt_rm_cont_pid(argv[0]);
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
	    (void) log_alert(0,"%s, shutdown pipe <%s> not able to catch answer)",
                                OPEP,cmd);
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
/*	Procedure to wait for container exit    */
/*      also check about task to do             */
/*						*/
/************************************************/
static int cont_attend(const char *contname,pid_t contpid)


{
#define OPEP    PRG":cont_attend"

int status;
int sleep_ms;   //waiting time in ms
int kill_mode;  //0, No kill, 1 SIGPWR kill, 2 TERM Kill,  3 KILL Kill
_Bool attend;
pid_t mpid;
STATYP *contstat;
struct pollfd pfd_struct;
int pfd;
int phase;
_Bool proceed;

status=0;
sleep_ms=5000;  //waiting 5 seconds
kill_mode=0;
attend=true;
mpid=(pid_t)0;
contstat=(STATYP *)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //getting the monitoring process PID
      for (int i=0;i<5;i++) {
        (void) sleep(1);
        (void) apl_checksig();    //purge signal
        if ((mpid=cnt_get_monitoring_pid(contpid))!=(pid_t)0) 
          break;        //we found the monitoring process PID
        }
      if (mpid==(pid_t)0) {
        (void) log_alert(0,"%s unable to get  monitor pid for container <%s> (%s)",
                            OPEP,contname,"Bug system? Abort!");
        goto TOOBAD;
        }
      break;
    case 1      :       //empty
      break;
    case 2      :       //opening the container pid file
      if ((pfd=pidfd_open(contpid,0))<0) {
        (void) log_alert(0,"%s unable open container <%s> pid file (error=<%s>)",
                             OPEP,contname,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 3      :       //check if container is alive
      (void) apl_checksig();    //purge old signal
      pfd_struct.fd=pfd;
      pfd_struct.events=POLLIN;
      while (attend==true) {
        time_t curtime;
        sigset_t no_mask;
        struct timespec timeout;
        int ready;

        (void) sigemptyset(&no_mask);
        timeout.tv_sec=sleep_ms/1000;
        timeout.tv_nsec=(sleep_ms%1000)*1000000;
        ready=ppoll(&pfd_struct,1,&timeout,&no_mask);
        curtime=time((time_t)0);
        switch (ready) {
          case -1       :       //trouble
            switch (errno) {
              case EINTR:       //got en interrupt
                (void) apl_checksig();
                if (sigterm==true)
                  sigint=true;
                if ((sigint==true)||(sighup==true)) {
                  char *argv[2];

                  kill_mode=1;
                  (void) log_alert(0,"%s Requesting \"powerof\" to container <%s>",
                                      OPEP,contname);
                  argv[0]=(char *)contname;
                  argv[1]=(char *)0;
                  (void) cont_shutdown(2,argv);
                  //(void) kill(contpid,SIGRTMIN+4);
                  sleep_ms=10000;       //waiting 10 second at most
                  }
                break;  
              default   :
                (void) log_alert(0,"%s unexpected pool error (error=<%s>)",
                                    OPEP,strerror(errno));
                attend=false;       
                break;
              }
            break;
          case  0       :       //standard timeout
            (void) log_alert(2,"%s container <%s> pid='%d' top synchro <%s>",
                                OPEP,contname,contpid,apl_ascsystime(curtime));
            switch (kill_mode) {
              case 0    :       //duty task
                if (contstat==(STATYP *)0) 
                  contstat=sys_new_cont_status(contpid);
                if (sys_update_cont_status(contstat)==true) {
                  (void) cnt_updateproc(contstat);
                  }
                if (kill(mpid,0)<0) {
                  (void) log_alert(0,"%s container <%s> monitoring lost! "
                                     "(error=<%s>",OPEP,contname,strerror(errno));
                  sleep_ms=1000;        //waiting 1 second at most
                  kill_mode++;
                  (void) log_alert(0,"%s Stopping container <%s> via SIGINT",
                                      OPEP,contname);
                  }
                break;
              case 1    :       //Humm timeout on soft kill
                kill_mode++;
                (void) log_alert(0,"%s Overkill container <%s> via SIGINT",
                                    OPEP,contname);
                (void) kill(contpid,SIGINT);
                break;
              case 2    :       //Humm timeout on SIGTERM?
                kill_mode++;
                (void) log_alert(0,"%s Sure-kill container <%s> via SIGKILL",
                                    OPEP,contname);
                (void) kill(contpid,SIGKILL);
                attend=false;
                break;
              default   :       //case never reachacble
                (void) log_alert(0,"%s Kill mode='%d' (Bug?)",OPEP,kill_mode);
                attend=false;
                break;
              }
            break;
          case 1        :       //Container exited
            attend=false;       
            break;
          }
        }
      close(pfd);
      contstat=sys_free_cont_status(contstat);
      break;
    case 4      :       //container is dead (or almost dead)
      if (kill_mode<3) {
        pid_t exited_pid;

        exited_pid=waitpid(contpid,&status,0);
        switch (exited_pid) {
          case -1       :       //error
            (void) log_alert(0,"%s Unexpected waitpid status (error=<%s>)",
                                OPEP,strerror(errno));
            if (WIFSIGNALED(status)!=0) {
              (void) log_alert(0,"%s exit signal='%d'",OPEP,WTERMSIG(status));
              status=WTERMSIG(status);
              }
            break;
          case 0        :       //container not found exited
            (void) log_alert(0,"%s Container <%s> found still up and running!",
                                OPEP,contname);
            break;
          default       :       //container exited on purpose
            (void) log_alert(2,"%s container <%s> exited on purpose",OPEP,contname);
            if (WIFEXITED(status)!=0) 
              (void) log_alert(3,"%s container <%s> exit code='%d'",
                                  OPEP,contname,WEXITSTATUS(status));
            if (WIFSIGNALED(status)!=0) {
              (void) log_alert(3,"%s container <%s> exit signal='%d'",
                                  OPEP,contname,WTERMSIG(status));
              status=WTERMSIG(status);
              }
            break;
          }
        (void) log_alert(2,"%s container exit status='%d'",OPEP,status);
        }
      break;
    case 5      :       //make sure there is no remaining process
      (void) prc_nozombie();
      break;
    case 6      :       //container is dead, removing file first.pid
      if (cnt_rm_cont_pid(contname)==false) {
        (void) log_alert(0,"%s unable to remove container <%s> pid file (%s)",
                                  OPEP,contname,"system?");
        goto TOOBAD;
        }
      break;
    TOOBAD      :
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
if (sighup==true)       //container reboot requested
  status=1;
return status;

#undef  OPEP
}
/*
^L
*/
/************************************************/
/*						*/
/*	Procedure to track console and feed     */
/*      the supervisor console.                 */
/*						*/
/************************************************/
static _Bool track_console(CONTYP *conf,int master_fd,char *slave)

{
#define OPEP    PRG":track_console"
#define LOGGER  "[Logr]"

_Bool isok;
const char *contpath;
int fcd;
char console_log[1024];
char console_dev[1024];
int phase;
_Bool proceed;

isok=false;
contpath=sys_get_cont_path(conf->contname);
fcd=-1;
snprintf(console_log,sizeof(console_log),"%s/console",contpath);
snprintf(console_dev,sizeof(console_dev),"%s/dev/console",contpath);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //creating console log simple file
      if ((fcd=open(console_log,O_RDWR))<0) {
        (void) log_alert(0,"%s, unable to open <%s> (error=<%s>)",
                            OPEP,console_log,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 1      :       //link console to slave
      (void) close(fcd);
      if (mount(slave,console_dev,NULL,MS_BIND,NULL)<0) {
        (void) log_alert(0,"%s, unable to mount <%s> over <%s> (error=<%s>)",
                            OPEP,slave,console_dev,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 2      :       //link console to slave
      if ((conf->slave_fd=open(slave,O_RDWR|O_CLOEXEC))<0) {
        (void) log_alert(0,"%s unable to open <%s> (error=<%s>)",
                            OPEP,slave,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 3      :       //forking process to track console
      conf->logger_pid=fork();
      switch (conf->logger_pid) {
        case -1         :
          (void) log_alert(0,"%s unable to start logger process (error=<%s>)",
                             OPEP,strerror(errno));
          goto TOOBAD;
          break;
        case 0          :
          (void) prc_settitle("%s: container %s; console logger",
                               APPNAME,conf->contname);
          (void) close(conf->sync_pipe[0]); 
          (void) close(conf->sync_pipe[1]);
          (void) close(conf->slave_fd);
          (void) log_alert(1,"%s Step %2d: Logging process now starting...",
                              LOGGER,1);
          (void) pty_tracker(master_fd,console_log);
          (void) close(master_fd);
          (void) log_alert(1,"%s Step %2d: Logging process now terminated",
                              LOGGER,2);
          _exit(0);     //exiting from sub process
          break;
        default         :       //main process
          isok=true;
          break;
        }
      break;
    TOOBAD      :       //Trouble
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef  LOGGER
#undef  OPEP
}
/*
^L
*/
/************************************************/
/*						*/
/*	procedure to boot a container		*/
/*						*/
/************************************************/
static int doboot(CONTYP *cont)

{
#define OPEP    PRG":doboot"
#define CONT    "[Cont]"
#define LIBBIN  "/usr/libexec/"VZGOT"/bin"

int status;
int mfd;
const char *contpath;
char rootfs[600];
int phase;
_Bool proceed;

status=0;
mfd=-1;
contpath=sys_get_cont_path(cont->contname);
(void) snprintf(rootfs,sizeof(rootfs),"%s/%s",contpath,"rootfs");
(void) log_alert(1,"%s ---------------------------------------------",CONT);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case  0     :       //empty phase
      break;
    case  1     :       //set container arch
      (void) log_alert(1,"%s Step %2d: Setting container arch to<%s>",
                          CONT,phase,cont->contarch);
      if (setarch(cont->contarch)<0) {
        (void) log_alert(0,"%s Unable Unable to set container arch (abort!)",OPEP);
        goto TOOBAD;
        }
      break;
    case  2     :       //unsharing needed clone flags
      (void) log_alert(1,"%s Step %2d: Unsharing CLONE_NEWTIME and CLONE_NEWCGROUP",
                          CONT,phase);
      if (unshare(CLONE_NEWCGROUP|CLONE_NEWTIME)<0) {
        (void) log_alert(0,"%s Unable unshare NEWCGROUP and NEWTIME! (error=<%s>)",
                            OPEP,strerror(errno));
        status=-2;
        goto TOOBAD;
        }
      break;
    case  3     :       //assign signal to death detection
      (void) log_alert(1,"%s Step %2d: set PR_SET_PDEATHSIG",CONT,phase);
      if (prctl(PR_SET_PDEATHSIG,SIGRTMIN+4)<0) {
        (void) log_alert(0,"%s Unable to trap PR_SET_PDEATHSIG signa (error=<%s>)",
                            OPEP,strerror(errno));
        goto TOOBAD;
        }
      break;
    case  4     :       //set the container boot time
      (void) log_alert(1,"%s Step %2d: set container <%s> boot time",
                          CONT,phase,cont->contname);
      if (sys_set_boot_time()==false) {
        (void) log_alert(0,"%s, Unable to set container <%s> boot time (abort)",
                            OPEP,cont->exec_args[1]);
        goto TOOBAD;
        }
      break;
    case  5     :       //set '/' as private
      (void) log_alert(1,"%s Step %2d: make \"/\" as private",CONT,phase);
      if (mount("none","/",NULL,MS_REC|MS_PRIVATE,NULL)<0) {
        (void) log_alert(0,"%s Unable to make \"/\" private (error=<%s>)",
                            OPEP,strerror(errno));
        goto TOOBAD;
        }
      break;
    case  6     :       //create all directory within rootfs
      (void) log_alert(1,"%s Step %2d: Make sure rootfs is fully populated",
                          CONT,phase);
      if (cnt_set_rootfs_dir(cont->contname)==false) {
        (void) log_alert(0,"%s Unable to make a clean rootfs (abort!)",OPEP);
        goto TOOBAD;
        }
      break;
    case  7     :       //rootfs is an autonomos directory
      (void) log_alert(1,"%s Step %2d: Make rootfs as an autonomous mount points",
                          CONT,phase);
      if (mount(rootfs,rootfs,NULL,MS_BIND|MS_REC,NULL)<0) {
        (void) log_alert(0,"%s Unable to make rootfs Autonomous (error=<%s>",
                            OPEP,strerror(errno));
        goto TOOBAD;
        }
      break;
    case  8     :       //rootfs is an autonomos directory
      (void) log_alert(1,"%s Step %2d: Mounting container /proc, /sys, ...",
                          CONT,phase);
      if (cont->contname!=(char *)0) {  //always
        static struct {
                  const char *name;
                  const char *type;
                  }dirs[]={
                          {"proc","proc"},
                          {"sys","sysfs"},
                          {"run","tmpfs"},
                          {(const char *)0,(const char *)0}
                          };


        for (int i=0;dirs[i].name!=(const char *)0;i++) {
          char ppath[PATH_MAX];
          const char *type;


          (void) snprintf(ppath,sizeof(ppath),"%s/%s",rootfs,dirs[i].name);
          type=dirs[i].type;
          if (mount(type,ppath,type,MS_NOSUID|MS_NODEV|MS_NOEXEC,NULL)<0) {
            (void) log_alert(0,"%s Unable to mount container <%s> (error=<%s>",
                              OPEP,dirs[i].name,strerror(errno));
            goto TOOBAD;
            }
          }
        }
      break;
    case  9     :       //getting vzgot monitoring MFD
      (void) log_alert(1,"%s Step %2d: loading vzmon in memory",CONT,phase);
      if ((mfd=sys_get_memfd("/usr/libexec/vzgot/bin/vzmon","[vzgot]"))<0) {
        (void) log_alert(0,"%s Unable create vzmon memory image error=<%s>",
                              OPEP,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 10     :       //link both supervisor and container dev
      (void) log_alert(1,"%s Step %2d: Link Supervisor&Container /dev dir...",
                          CONT,phase);
      if (contpath!=(char *)0) { //always
        char src[1024];
        char dst[1024];

        (void) snprintf(src,sizeof(src),"%s/dev",contpath);
        (void) snprintf(dst,sizeof(dst),"%s/dev",rootfs);
        if (mount(src,dst,NULL,MS_BIND|MS_REC,NULL)<0) {
          (void) log_alert(0,"%s, unable to bind file <%s> to <%s> (error=<%s>)",
                              OPEP,src,dst,strerror(errno));
          goto TOOBAD;
          }
        }
      break;
    case 11     :       //mounting special directories /run
      break;
      (void) log_alert(1,"%s Step %2d: Mounting /run using tmpfs...",CONT,phase);
      if (rootfs!=(char *)0) { //always
        char src[1024];

        (void) snprintf(src,sizeof(src),"%s/run",rootfs);
        if (mount("tmpfs",src,"tmpfs",MS_NOSUID|MS_NODEV,"mode=755")<0) {
          (void) log_alert(0,"%s, unable to mount dir <%s> as tmpfs (error=<%s>)",
                              OPEP,src,strerror(errno));
          goto TOOBAD;
          }
        }
      break;
    case 12     :       //mounting cgroup in container
      (void) log_alert(1,"%s Step %2d: Mounting container cgroup...",CONT,phase);
      if (rootfs!=(char *)0) { //always
        char src[1024];

        (void) snprintf(src,sizeof(src),"%s/sys/fs/cgroup",rootfs);
        if (mount("cgroup2",src,"cgroup2",MS_NOSUID|MS_NODEV|MS_NOEXEC,NULL)<0) {
          (void) log_alert(0,"%s, unable to mount <%s> as cgroup2 (error=<%s>)",
                              OPEP,src,strerror(errno));
          goto TOOBAD;
          }
        }
      break;
    case 13     :       //set container architecture
      (void) log_alert(1,"%s Step %2d: Empty",CONT,phase);
      break;
    case 14     :       //Pivoting root
      (void) log_alert(1,"%s Step %2d: Pivoting root...",CONT,phase);
      if (chdir(rootfs)<0) {
        (void) log_alert(0,"%s Unable chdir to <%s> (error=<%s>)",
                            OPEP,rootfs,strerror(errno));
        goto TOOBAD;
        }
      if (pivot_root(".","old_root")<0) {
        (void) log_alert(0,"%s Unable pivot root (error=<%s>)",
                            OPEP,strerror(errno));
        goto TOOBAD;
        }
      if (chdir("/")<0) {
        (void) log_alert(0,"%s Unable chdir to \"/\" (error=<%s>)",
                            OPEP,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 15     :       //removing old root directory
      (void) log_alert(1,"%s Step %2d: removing old root...",CONT,phase);
      if (umount2("/old_root",MNT_DETACH)<0) {
        (void) log_alert(0,"%s Unable to umount old_root (error=<%s>)",
                            OPEP,strerror(errno));
        goto TOOBAD;
        }  
      if (rmdir("/old_root")<0) {
        (void) log_alert(0,"%s Unable to remove old_root directory (error=<%s>)",
                            OPEP,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 16     :       //executing the container monitoring
      (void) log_alert(1,"%s Step %2d: Start container monitoring...",CONT,phase);
      if (monitoring(mfd)==false) {
        (void) log_alert(0,"%s Unable to start container monitoring",OPEP);
        goto TOOBAD;
        }
      break;
    case 17     :       //executing the right script
      (void) log_alert(1,"%s Step %2d: Starting <%s> as PID 1 process...",
                          CONT,phase,cont->exec_args[0]);
      (void) sleep(2);
      (void) log_alert(1,"%s -----------------------------------------",CONT);
      if (rootfs!=(char *)0) {  //always
        char *container_env[] = {
            "PATH=/usr/sbin:/usr/bin:/sbin:/bin",
            "TERM=xterm-256color",
            "container=vzgot",
            "container_ttys=console",
            (char *)0
            };
        int target_fd;

        target_fd=open("/dev/console",O_RDWR);
        if (target_fd>=0) {
          // To replace stdin,stdout,stderr by console
          (void) dup2(target_fd,STDIN_FILENO); //container will 'read' from console
          (void) dup2(target_fd,STDOUT_FILENO); //container stdout
          (void) dup2(target_fd,STDERR_FILENO); // container stderr
          //security
          if (target_fd>2) {
            close(target_fd);
            }
          }
        else {  //can't not access console
         int null_fd;
         null_fd=open("/dev/null",O_RDWR);
          if (null_fd>=0) {
            (void) dup2(null_fd,STDIN_FILENO);
            (void) dup2(null_fd,STDOUT_FILENO);
            (void) dup2(null_fd,STDERR_FILENO);
            (void) close(null_fd);
            }
          }
        if (execvpe(cont->exec_args[0],cont->exec_args,container_env)<0) {
          (void) log_alert(0,"%s Unable to exec <%s> (error=<%s>)",
                             OPEP,cont->exec_args[0],strerror(errno));
          }
        goto TOOBAD;
        }
      break;
    TOOBAD      :
      status=1;         //reportint trouble
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return status;

#undef  LIBBIN
#undef  OPEP
}
/*
^L
*/
/************************************************/
/*						*/
/*	Procedure to do container fisrt step    */
/*      as as clone process.                    */
/*						*/
/************************************************/
static void start_clone(CONTYP *cont)

{
#define OPEP    PRG":start_clone"

int status;
int phase;
int ch;
_Bool proceed;

status=0;
ch='\000';
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //First close the not needed descriptor
      (void) close(cont->slave_fd);
      (void) close(cont->sync_pipe[1]);
      break;
    case 1      :       //wait form supervisor clearance
      if (read(cont->sync_pipe[0],&ch,1)!=1) {
        (void) log_alert(0,"%s No clearance to start from supervisor!",OPEP);
        status=-1;
        goto TOOBAD;
        }
      break;
    case 2      :       //starting the container work
      status=doboot(cont);
      break;
    TOOBAD      :
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
(void) _exit(status);

#undef OPEP
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
  status++;             //incrementing the failurs status number
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

        (void) snprintf(execpath,sizeof(execpath),"%s/bin/%s",libexecpath,VZSTAT);
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
#define OPEP PRG":boot"
#define HST "[Host]"

static char *default_args[] = {"/sbin/init",NULL};

int status;
char *name;
CONTYP *cont;
int pty_master_fd;
pid_t child_pid;
char *pty_slave_name;
struct clone_args c_args;
int phase;
_Bool proceed;

status=0;
name=argv[0];
cont=(CONTYP *)calloc(1,sizeof(CONTYP));
(void) snprintf(cont->contname,sizeof(cont->contname),"%s",name);
cont->exec_args=default_args;
pty_master_fd=-1;
child_pid=(pid_t)0;
pty_slave_name=(char *)0;
(void) memset(&c_args, 0, sizeof(c_args));
c_args.exit_signal=SIGCHLD;
c_args.flags|=CLONE_NEWNET;
c_args.flags|=CLONE_NEWIPC;
c_args.flags|=CLONE_NEWNS;
c_args.flags|=CLONE_NEWPID;
c_args.flags|=CLONE_NEWUTS;
//c_args.flags|=CLONE_NEWTIME;
//Do no set CLONE_NEWTIME, use unshare(CLONE_NEWTIME) within container
//c_args.flags|=CLONE_NEWCGROUP;
//Same for CLONE_NEWCGROUP, use unshare(CLONE_NEWCGROUP)
c_args.flags|=CLONE_NEWUSER;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case  0	:	//opening the synchronization pip
      (void) log_alert(0,"%s Step %2d: Starting container <%s>...",HST,phase,name);
      if (argc>1)       //specific exec set
        cont->exec_args=&argv[1];
      if (cfg_loadconfig((const char *)0,name)<0) {
        (void) log_alert(0,"%s, Unable to load configuration (Abort!)",OPEP);
        goto TOOBAD;
        }
      if (cont!=(CONTYP *)0) {   //always
        const char *arch;
        const char *dist;

        arch=cnt_getarch(cont->contname);
        dist=cnt_getdist(cont->contname);
        (void) snprintf(cont->contarch,sizeof(cont->contarch),"%s",arch);
        (void) snprintf(cont->contdist,sizeof(cont->contdist),"%s",dist);
        }
      break;
    case  1	:	//supervisor set in backgroup and locking
      (void) log_alert(1,"%s Step %2d: Locking container...",HST,phase);
      (void) prc_divedivedive(foreground,appname);
      if (lck_locking(argv[0],LCK_LOCK,5)==false) {
        (void) log_alert(0,"Unable to lock container <%s> access",argv[0]);
        status=phase;
        proceed=false;  /*trouble trouble       */
        }
      break;
    REBOOT      :       //entry phase in case of container reboot
      sighup=false;      //Making sure getting new INT or HUP signal
      sigint=false;     
      phase=2;          //Restarting phase
    case  2	:	//Making sure the rootfs contents is still 
                        //container compatible
      if (cnt_initscript(VZBOOT,"%s %s",cont->contname,cont->contdist)==false) {
        (void) log_alert(0,"%s, Init Script <%s> failure for container <%s>",
                            OPEP,VZBOOT,cont->contname);
        goto TOOBAD;
        }
      break;
    case  3	:	//opening the pty to redirect container console
      (void) log_alert(1,"%s Step %2d: Opening console pipe...",HST,phase);
      if (pipe(cont->sync_pipe)<0) {
        (void) log_alert(0,"%s, Can not create sync pipe (error=<%s>)",
                            OPEP,strerror(errno));
        goto TOOBAD;
        }
      break;
    case  4	:	//opening the pty to redirect container console
      (void) prc_settitle("%s: container %s (%s/%s) up",
                           APPNAME,name,cont->contdist,cont->contarch);
      (void) log_alert(1,"%s Step %2d: Init master pty...",HST,phase);
      if ((pty_master_fd=posix_openpt(O_RDWR|O_NOCTTY))<0) {
        (void) log_alert(0,"%s, Can not open a pseudo terminal (error=<%s>)",
                            OPEP,strerror(errno));
        goto TOOBAD;
        }
      if (grantpt(pty_master_fd)<0) {
        (void) log_alert(0,"%s, Can not grant access pterminal slave (error=<%s>)",
                            OPEP,strerror(errno));
        goto TOOBAD;
        }
      if (unlockpt(pty_master_fd)<0) { 
        (void) log_alert(0,"%s, Can not grant unlock pseudo terminal (error=<%s>)",
                            OPEP,strerror(errno));
        goto TOOBAD;
        }
      pty_slave_name=ptsname(pty_master_fd);
      break;
    case  5	:	//set cgroup
      (void) log_alert(1,"%s Step %2d: Prepare Cgroup...",HST,phase);
      if (cnt_set_cgroup(name)==false) {
        (void) log_alert(0,"%s, Unable to set cgroup...",OPEP);
        goto TOOBAD;
        }
      break;
    case  6	:	//set devices
      (void) log_alert(1,"%s Setp %2d: Prepare Container /dev...",HST,phase);
      if (cnt_set_sup_devices(cont->contname)==false) {
        (void) log_alert(0,"%s, Unable to set container devices...",OPEP);
        goto TOOBAD;
        }
      break;
    case  7	:	//Prepare console monitoring
      (void) log_alert(1,"%s Step %2d: Starting Container console TTY...",
                          HST,phase);
      if (track_console(cont,pty_master_fd,pty_slave_name)==false) {
        (void) log_alert(0,"%s, Unable track console...(abort)",OPEP);
        goto TOOBAD;
        }
      break;
    case  8	:	//start container process
      (void) log_alert(1,"%s Step %2d: Starting Container process...",HST,phase);
      child_pid=syscall(SYS_clone3,&c_args,sizeof(c_args));
      switch (child_pid) {
        case -1 :       //unable to start container process
          (void) log_alert(0,"%s, Unable to start container process (error=<%s>)",
                              OPEP,strerror(errno));
          goto TOOBAD;
          break;
        case  0 :       //container process itself;
          (void) close(pty_master_fd);
          (void) start_clone(cont);
          //container process exited, never ever reached
          break;
        default :       //Supervisor process
          (void) close(cont->sync_pipe[0]);
          break;
        }
      break;
    case  9     :
      (void) log_alert(0,"%s Step %2d: Container '%s' started with PID='%d'...",
                          HST,phase,cont->contname,child_pid);
      if (cnt_set_cont_pid(cont->contname,child_pid)==false) {
        (void) log_alert(0,"%s, Starting failure for container <%s> (Abort!)",
                            OPEP,name);
        goto TOOBAD;
        }
      if (sys_set_start()==false) {
        (void) log_alert(0,"%s, Unable to set start time for container <%s> (%s)",
                            OPEP,name,"Abort!");
        goto TOOBAD;
        }
      break;
    case 10	:	//set the container pid with the right cgroups
      (void) log_alert(1,"%s Step %2d: managing cgroup",HST,phase);
      (void) sys_move_to_cgroup(cont->contname,cgr_containers,child_pid);
      (void) cnt_mapcontids(cont->contname,child_pid,0,0);
      break;
    case 11	:	//establishing the network part
      (void) log_alert(1,"%s Step %2d: set container <%s> network part",
                          HST,phase,name);
      if (cnt_initscript(VZETHUP,"%s %d",name,child_pid)==false) {
        (void) log_alert(0,"%s, Init Script <%s> failure for container <%s>",
                            OPEP,VZETHUP,name);
        goto TOOBAD;
        }
      break;
    case 12	:	//synchronising with container
      (void) log_alert(1,"%s Step %2d: Give clearance to start to container <%s>",
                          HST,phase,name);
      if (write(cont->sync_pipe[1],"1",1)!=1)
        (void) log_alert(0,"%s, Unable to send clearance to container (error=<%s>)",
                            OPEP,strerror(errno));
      close(cont->sync_pipe[1]);
      break;
    case 13	:	//waiting container exit adn do supervision tasks
      (void) log_alert(1,"%s Step %2d: Waiting for container <%s> exit",
                          HST,phase,name);
      status=cont_attend(name,child_pid);
      (void) log_alert(1,"%s Step %2d: Completed, container <%s> exited "
                         "with status='%d'",HST,phase,name,status);
      break;
    case 14	:	//waiting container exit and do supervision tasks
      (void) log_alert(1,"%s Step %2d: remove container <%s> network part",
                          HST,phase,name);
      if (cnt_initscript(VZETHDOWN,"%s",name)==false) {
        (void) log_alert(0,"%s, Init Script <%s> failure for container <%s>",
                            OPEP,VZETHDOWN,name);
        goto TOOBAD;
        }
      break;
    case 15	:	//waiting container exit and do supervision tasks
      (void) log_alert(1,"%s Step %2d: Unmounting container <%s>",HST,phase,name);
      (void) cnt_unset_rootfs_dir(cont->contname);
      break;
    TOOBAD      :
      phase=16;         //making sure the phase value is right
    case 16	:	//removing cgroup
      (void) log_alert(1,"%s Step %2d: Freeing console PTY device",HST,phase);
      (void) close(cont->slave_fd);
      (void) close(pty_master_fd); 
      (void) waitpid(cont->logger_pid,NULL,0);
      //Sync PIPE not needed any more
      for (int i=0;i<2;i++) {
        if (cont->sync_pipe[i]>=0) {
          close(cont->sync_pipe[1]);
          cont->sync_pipe[1]=-1;
          }
        }
      break;
    case 17	:	//set the container pid with the right cgroups
      (void) log_alert(1,"%s Step %2d: Cleaning container <%s> cgroups",
                          HST,phase,name);
      (void) sys_move_to_cgroup(cont->contname,cgr_top,getpid());
      (void) sys_clean_all_cgroup(cont->contname,cgr_containers);
      (void) sys_clean_all_cgroup(cont->contname,cgr_supervisors);
      break;
    case 18	:	//set the container pid with the right cgroups
      (void) log_alert(1,"%s Step %2d: umounting supervisor dev",HST,phase);
      if (cnt_unset_sup_devices(cont->contname)==false)
        phase=999;      //Small trouble
      break;
    case 19	:	//do we need to restart container
      (void) log_alert(1,"%s Step %2d: Check restart status",HST,phase);
      switch (status) {
        case 1  :       //Need to reboot container
          (void) log_alert(0,"%s Step %2d: Rebooting container <%s>",
                              OPEP,phase,name);
          goto REBOOT;
          break;
        case 2  :       //Standard poweroff
          break;
        default :       //Unexpected status
          (void) log_alert(0,"%s Step %2d: unexpected container <%s> status='%d'",
                              OPEP,phase,name,status);
          break;
        }
      break;
    case 20	:	//unlocking
      (void) log_alert(1,"%s Step %2d: Unlocking supervisor",HST,phase);
      if (lck_locking(argv[0],LCK_UNLOCK,1)==false) {
        (void) log_alert(1,"%s, Unable to unclock container <%s>",OPEP,name);
        }
      break;
    case 21	:	//last phase
      (void) log_alert(0,"%s Step %2d: Container <%s> all processes down. "
                         "Craft securely docked.",HST,phase,name);
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
(void) free(cont);
return status;

#undef  HST
#undef  OPEP
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
	(void) fprintf(stderr,"reboot is unable to shutdown container <%s>\n",
                                argv[0]);
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
	a_boot,		/*booting container		*/
	a_onboot,	/*boot container when host start*/
	a_offboot,	/*do not boot container at boot */
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
	{a_boot,	"boot",		(const void *)0},
	{a_offboot,	"offboot",	(const void *)0},
	{a_onboot,	"onboot",	(const void *)0},
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
      case a_boot	:	
         status=boot(argc-1,argv+1);
        break;
      case a_onboot	:	
        status=vzscript(VZON,argc-1,argv+1);
        break;
      case a_offboot	:	
        status=vzscript(VZOFF,argc-1,argv+1);
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
while ((c=getopt(argc,argv,"+c:d:UfhVv"))!=EOF) {
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
    case   'V'  :
      (void) fprintf(stderr,"%s: Version %s\n",appname,apl_getvers());
      status=1;
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
