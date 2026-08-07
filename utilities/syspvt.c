// vim: smarttab tabstop=8 shiftwidth=2 expandtab

#include <linux/sched.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sched.h>
#include <syscall.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <ftw.h>
#include <poll.h>
#include <pty.h>

#define PRG             "syspivot.c"

#define CONTNAME        "dummy3"
#define APP             "vzgot"
#define	DUMMY_ROOT	"/vzgot/test/"CONTNAME"/rootfs"
#define SYSFS           "/sys/fs/cgroup/"APP


//defining cgroup sub-section
static  const char *subcont[]={
        "supervisors",
        "containers",
        (const char *)0
        };

//superviseur/container synchronisation
#define CON_PIPE 0      //Container Channel
#define SUP_PIPE 1      //Supervisor Channel
static  int     synchro[2][2];

//synchronisation protocol
typedef enum    {
    SYNC_START      = 'S',      //container start
    SYNC_DEV_READY  = 'D',      //Container say dev is ready
    SYNC_PTY_DONE   = 'P',      //superviseur say PTY done
    SYNC_ERR        = 'E'       //Trouble
    }sync_start_t;

/*

*/
/************************************************/
/*						*/
/*	Defining pivot_root system call		*/
/*						*/
/************************************************/
static int pivot_root(const char * new_root,const char * put_old)

{
#ifndef __NR_pivot_root
#pragma message ("the pivot_root syscall is not available within \"sys/syscall.h\"")
#pragma message ("-> The pivot_root system call will generate an alert")
(void) log_alert(0,"%s need the pivot_root system call!!!.",appname);
errno=ENOSYS;
return -1;
#else
return(syscall(__NR_pivot_root, new_root, put_old));
#endif
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to open the synchro pipe      */
/*      on a synchronisation PIP channel        */
/*						*/
/************************************************/
static _Bool open_synchro()

{
#define OPEP    PRG":open_synchro"
_Bool isok;
int channel;

isok=true;
channel=CON_PIPE;
while (channel<=SUP_PIPE) {
  if (pipe(synchro[channel])<0) {
    (void) fprintf(stderr,"%s Unable to open pipe channel='%d' (error=<%s>\n",
                           OPEP,channel,strerror(errno));
    isok=false;
    break;      //Big trouble no need to go further
    }
  channel++;
  }
return isok;

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to wait an incoming character */
/*      on a synchronisation PIP channel        */
/*						*/
/************************************************/
static char read_pipe(int channel)

{
#define OPEP    PRG":read_pipe"

char got;
char chr;
int nbr;

chr='\000';
got=SYNC_ERR;
nbr=read(synchro[channel][0],&chr,1);
switch (nbr) {
  case -1       :       //trouble?
    (void) fprintf(stderr,"%s unable read pipe on channel='%d' "
                          "(error=<%s> System?)\n",
                           OPEP,channel,strerror(errno));
    break;
  case  0       :       //No character recieved??
    (void) fprintf(stderr,"%s no char available on pipe channel='%d'(Bug?)\n",
                           OPEP,channel);
    break;
  default       :       //everything Right, got one char
    got=chr; 
    break;
  }
return got;

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to write a character to a     */
/*      synchronisation PIP channel             */
/*						*/
/************************************************/
static int write_pipe(int channel,char cmd)

{
#define OPEP    PRG":write_pipe"

int status;
int nbr;
char seq[8];

status=-1;
seq[0]=cmd;
seq[1]='\000';
nbr=write(synchro[channel][1],seq,1);
switch (nbr) {
  case -1       :       //trouble?
    (void) fprintf(stderr,"%s unable write pipe on channel='%d' "
                          "(error=<%s> System?)\n",
                           OPEP,channel,strerror(errno));
    break;
  case  0       :       //No character recieved??
    (void) fprintf(stderr,"%s unable to write to on pipe channel='%d'(Bug?)\n",
                           OPEP,channel);
  default       :       //everything Right, wrote one char
    status=0;
    break;
  }
return status;

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to remove a file from BOTTOM	*/
/*	to TOP.					*/
/*						*/
/************************************************/
static int nftw_cleanup_cb(const char *fpath,const struct stat *sb,
			   int typeflag,struct FTW *ftwbuf)

{
//removing only (cgroup) directory for now
int status;

status=0;
if (typeflag==FTW_DP) 
  status=rmdir(fpath);
return status;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to create a directory         */
/*	to TOP.					*/
/*						*/
/************************************************/
static int do_dir(const char *dirname,mode_t mode)

{
#define OPEP    PRG":do_dir"

int status;

status=0;
if ((status=mkdir(dirname,mode))<0) {
  switch (errno) {
    case EEXIST   :       //Nothing to do
      status=0;
      break;
    default       :
      (void) fprintf(stderr,"%s unable to create directory <%s> (error=<%s>)\n",
                             OPEP,dirname,strerror(errno));
       break;
    }
  }
return status;

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to write a line to file	*/
/*						*/
/************************************************/
static int write_file(const char *path,const char *line)

{
#define OPEP    PRG":write_file"

int status;
int done;
int fd;
int phase;
_Bool proceed;

status=-1;
fd=-1;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //Opening the path
      if ((fd=open(path,O_WRONLY))<0) {
        (void) fprintf(stderr,"%s unable to open file <%s> (error=<%s>)\n",
                               OPEP,path,strerror(errno));
        phase=999;
        }
      break;
    case 1      :       //writing string
      if ((done=write(fd,line,strlen(line)))!=(ssize_t)strlen(line)) {
        (void) fprintf(stderr,"%s unable to fully write <%s> to "
                              "file <%s>, only %d/%lu (error=<%s>)\n",
                              OPEP,line,path,done,strlen(line),strerror(errno));
        phase=999;
        }
      (void) close(fd);
      break;
    case 2      :       //everything fine.
      status=0;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return status;

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	To mount the superviseur PTY master     */
/*      device to container /dev/console        */
/*						*/
/************************************************/
static _Bool mounting_pty(char *slave_name,pid_t cont_pid)

{
#define OPEP    PRG":mounting_pty"

_Bool isok;
char conpath[PATH_MAX];

isok=true;
(void) snprintf(conpath,sizeof(conpath),"/%s/dev/console",DUMMY_ROOT);
if (mount(slave_name,conpath,NULL,MS_BIND,NULL)<0) {
  (void) fprintf(stderr,"%s unable to bind <%s> to <%s> (error=<%s>)\n",
                         OPEP,slave_name,conpath,strerror(errno));
  sleep(30);
  isok=false;
  }
return isok;

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to informe supervisor the     */
/*      /dev is ready and wait for pty ack.     */
/*						*/
/************************************************/
static _Bool dev_now_ready() 

{
#define OPEP    PRG":dev_now_ready"

_Bool isok;
char chr;
int phase;
_Bool proceed;

isok=true;
chr='\000';
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //Sending device ready to supervisor
      (void) fprintf(stdout,"%s JMPDB sleep \n",OPEP);
      (void) sleep(2);  //JMPDBG
      if (write_pipe(SUP_PIPE,SYNC_DEV_READY)<0) {
        (void) fprintf(stderr,"%s Unable to send to <%s> supervisor (error=<%s>)\n",
                                 OPEP,CONTNAME,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 1      :       //container all device ready
      (void) fprintf(stdout,"%s JMPDB waiting for PTY DONE \n",OPEP);
      chr=read_pipe(CON_PIPE);
      switch (chr) {
        case SYNC_PTY_DONE      :       //everything fine
          break;
        case SYNC_ERR           :
          (void) fprintf(stderr,"%s no syn_pty_done (Abort!)\n",OPEP);
          goto TOOBAD;
          break;
        default         :
          (void) fprintf(stderr,"%s Unexpected sync char='%c' (Bug?)\n",OPEP,chr);
          goto TOOBAD;
          break;
        }
      (void) fprintf(stdout,"%s JMPDB GOT PTY DONE \n",OPEP);
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

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	To Make sure the apparmor profile is    */
/*      linked to container                     */
/*						*/
/************************************************/
static int apply_apparmor_profile(const char *profile_name) 

{
#define SELFX   "/proc/self/attr/exec"

char buf[256];

snprintf(buf,sizeof(buf),"exec %s",profile_name);
return write_file(SELFX,buf);

#undef  SELFX
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to prepare all cgroup         */
/*      directories.                            */
/*						*/
/************************************************/
static _Bool do_sys_cgroup(const char *contname)

{
#define OPEP    PRG":do_sysgroup"

_Bool isok;
int phase;
_Bool proceed;

isok=true;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //Creating the cgroup application directory
      if (do_dir(SYSFS,0755)<0) 
        goto TOOBAD;
      break;
    case 1      :       //setting control
      if (write_file(SYSFS"/cgroup.subtree_control","+cpu +memory +pids")<0) {
        (void) fprintf(stderr,"%s unable to assign control (%s)\n",OPEP,"Abort!");
        goto TOOBAD;
        }  
      break;
    case 2      :       //creation all cgroups needed directories
      for (int i=0;subcont[i]!=(char *)0;i++) {
        char dirloc[PATH_MAX];

        (void) snprintf(dirloc,sizeof(dirloc),"%s/%s",SYSFS,subcont[i]);
        if (do_dir(dirloc,0755)<0) 
          goto TOOBAD;
        }
      break;
    case 3      :       //creation all application needed directories
      for (int i=0;subcont[i]!=(char *)0;i++) {
        char dirloc[PATH_MAX];

        (void) snprintf(dirloc,sizeof(dirloc),"%s/%s/%s",SYSFS,subcont[i],contname);
        if (do_dir(dirloc,0755)<0) 
          goto TOOBAD;
        }
      break;
    case 4      :       //Position supervisor components in the right cgroup
      if (subcont[0]!=(char *)0) {      //always
        char mypid[32];
        char ppath[PATH_MAX];

        (void) snprintf(mypid,sizeof(mypid),"%d\n",getpid());
        (void) snprintf(ppath,sizeof(ppath),"%s/%s/%s/cgroup.procs",
                                            SYSFS,subcont[0],contname);
        if (write_file(ppath,mypid)<0) {
          (void) fprintf(stderr,"%s unable to assign supervisor cgroup (%s)\n",
                              OPEP,"Abort!");
          goto TOOBAD;
          }
        }
      break;
    TOOBAD      :       //Trouble trouble
      isok=false;       //NO BREAK
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to release all cgroup         */
/*      directories.                            */
/*						*/
/************************************************/
static _Bool undo_sys_cgroup(const char *contname)

{
#define OPEP    PRG":undo_sys_cgroup"

_Bool isok;
int retries;
char cgroup[64];
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=true;
retries=-1;
(void) snprintf(cgroup,sizeof(cgroup),"%s/%s",subcont[1],contname);
ppath[0]='\000';
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //send global terination to all cgroup process.
      (void) snprintf(ppath,sizeof(ppath),"%s/%s/cgroup.kill",SYSFS,cgroup);
      if (write_file(ppath,"1")<0) {
        (void) fprintf(stderr,"%s unable to send termination signal to <%s>\n",
                              OPEP,ppath);
        goto TOOBAD;
        }
      break;
    case 1      :       //removing all sub directory
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",SYSFS,cgroup);
      retries=100;
      while (retries>0) {
        int nbr;

        retries--;
        nbr=nftw(ppath,nftw_cleanup_cb,10,FTW_DEPTH|FTW_PHYS);
        switch (nbr) {
          case -1       :               //trouble
            (void) fprintf(stderr,"%s unable to clean cgroup <%s> (error=<%s>)\n",
                                   OPEP,ppath,strerror(errno));
            (void) usleep(20000);       //lets relax 20 ms
            break;                      //lets try again
          default       :
            (void) fprintf(stderr,"%s nftw Unexpected value '%d' (Bug?)\n",
                                   OPEP,nbr);
            //NO break, exiting in case of default
          case 0        :       //Everything done
            retries=0;
            break;              //lets try again
          }
        }
      break;
    case 2      :       //Moving superviseur to upper level
      if (subcont[0]!=(char *)0) {      //always
        char mypid[32];

        (void) snprintf(mypid,sizeof(mypid),"%d\n",getpid());
        (void) snprintf(ppath,sizeof(ppath),"%s/%s/cgroup.procs",SYSFS,subcont[0]);
        if (write_file(ppath,mypid)<0) {
          (void) fprintf(stderr,"%s unable to assign supervisor cgroup (%s)\n",
                              OPEP,"Abort!");
          goto TOOBAD;
          }
        }
      break;
    case 3      :       //removing all supervisors/'cont-name'
      (void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",SYSFS,subcont[0],contname);
      if (rmdir(ppath)<0) {
        (void) fprintf(stderr,"%s unable to remove cgroup <%s> (error=<%s>)\n",
                                 OPEP,ppath,strerror(errno));
        goto TOOBAD;
        }
      break;
    TOOBAD      :       //trouble
      isok=false;       //NO break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to mount all file-systeme     */
/*      needed by container.                    */
/*						*/
/************************************************/
static _Bool do_sysmount()

{
#define OPEP    PRG":do_sysmount"
#define DEVPTS  "/dev/pts"

static const char *dirs[]={
        "/proc",
        "/sys",
        "/dev",
        (const char *)0
        };
typedef struct  {
        const char *name;
        int mode;
        int major;
        int minor;
        }DEVTYP;

static  DEVTYP devs[]={
        {"null",0666,1,3},
        {"zero",0666,1,5},
        {"random",0666,1,8},
        {"urandom",0666,1,9},
        {(const char *)0,0,0,0}
        };


_Bool isok;
int mntflags;
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=true;
mntflags=0;
ppath[0]='\000';
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //Creating all needed directory
      for (int i=0;dirs[i]!=(const char *)0;i++) {
        (void) snprintf(ppath,sizeof(ppath),"%s/%s",DUMMY_ROOT,dirs[i]);
        if ((mkdir(ppath,0755)<0)&&(errno!=EEXIST)) {
          (void) fprintf(stderr,"%s unable to create directory <%s> (error=<%s>)\n",
                                 OPEP,ppath,strerror(errno));
          goto TOOBAD;
          }  
        } 
      break;
    case 1      :       //mounting "/proc"
      mntflags=MS_NOSUID|MS_NODEV|MS_NOEXEC;
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",DUMMY_ROOT,dirs[0]);
      if (mount("proc",ppath,"proc",mntflags,NULL)<0) {
        (void) fprintf(stderr,"%s unable to mount directory <%s> (error=<%s>)\n",
                               OPEP,ppath,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 2      :       //mounting "/sys"
      mntflags=MS_NOSUID|MS_NODEV|MS_NOEXEC|MS_RDONLY;
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",DUMMY_ROOT,dirs[1]);
      if ( mount("sysfs",ppath,"sysfs",mntflags,NULL)<0) {
        (void) fprintf(stderr,"%s unable to mount directory <%s> (error=<%s>)\n",
                               OPEP,ppath,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 3      :       //mounting "/dev" in tmpfs mode
      mntflags=MS_NOSUID|MS_STRICTATIME;
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",DUMMY_ROOT,dirs[2]);
      if (mount("tmpfs",ppath,"tmpfs",mntflags,"mode=755")<0) {
        (void) fprintf(stderr,"%s unable to mount directory <%s> (error=<%s>)\n",
                               OPEP,ppath,strerror(errno));
        goto TOOBAD;
        }  
      break;
    case 4      :       //Make sur /dev is private
      mntflags=MS_PRIVATE;
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",DUMMY_ROOT,dirs[2]);
      if (mount(NULL,ppath,NULL,mntflags,NULL)<0) {
        (void) fprintf(stderr,"%s unable to set directory <%s> as private "
                              "(error=<%s>)\n",OPEP,ppath,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 5      :       //creating directory /dev/pts
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",DUMMY_ROOT,DEVPTS);
      if ((mkdir(ppath,0755)<0)&&(errno!=EEXIST)) {
        (void) fprintf(stderr,"%s unable to create directory <%s> (error=<%s>)\n",
                                 OPEP,ppath,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 6      :       //mounting "/dev/pts" in tmpfs mode
      mntflags=0;
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",DUMMY_ROOT,DEVPTS);
      if (mount("devpts",ppath,"devpts",mntflags,"newinstance,ptmxmode=0666")<0) {
        (void) fprintf(stderr,"%s unable to mount directory <%s> (error=<%s>)\n",
                               OPEP,ppath,strerror(errno));
        goto TOOBAD;
        }  
      break;
    case 7      :       //creating all devices
      for (int i=0;devs[i].name!=(const char *)0;i++) {
        dev_t majmin;

        majmin=makedev(devs[i].major,devs[i].minor);
        (void) snprintf(ppath,sizeof(ppath),"%s/dev/%s",DUMMY_ROOT,devs[i].name);
        if (mknod(ppath,S_IFCHR|devs[i].mode,majmin)<0) {
          (void) fprintf(stderr,"%s unable device <%s> (error=<%s>)\n",
                               OPEP,ppath,strerror(errno));
          goto TOOBAD;
          }
        }
      break;
    case 8      :       //creating a console device
      {
      int fd;

      (void) snprintf(ppath,sizeof(ppath),"%s/dev/%s",DUMMY_ROOT,"console");
      if ((fd=open(ppath,O_WRONLY|O_CREAT|O_CLOEXEC,0644))<0) {
        (void) fprintf(stderr,"%s unable open device <%s> (error=<%s>)\n",
                               OPEP,ppath,strerror(errno));
        goto TOOBAD;
        }
      (void) close(fd);
      }
      break;
    TOOBAD      :
      isok=false;       //report problem
                        //NO BREAK
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  (void) fprintf(stderr,"%s JMPDBG ppath=<%s>\n",OPEP,ppath);
  phase++;
  }
return isok;

#undef  DEVPTS
#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to check if container is still*/
/*      alive and report report console data.   */
/*						*/
/************************************************/
static int check_container(pid_t cont_pid,int master_fd)

{
#define OPEP    PRG":check_container"

int status;
_Bool isok;
struct pollfd fds[1];

status=0;
fds[0].fd=master_fd;
fds[0].events=POLLIN;
isok=true;
while (isok==true) {
  int ret;
  pid_t dead_pid;
  ssize_t num_bytes;
  char buffer[4096];

  buffer[0]='\000';
  //waiting at most 50 ms to display console log quickly
  ret=poll(fds,1,50);
  if ((ret>0)&&(fds[0].revents & POLLIN)) {
    num_bytes=read(master_fd,buffer,sizeof(buffer)-1);
    if (num_bytes>0) {
      buffer[num_bytes]='\0';
      (void) fprintf(stdout,"%s <%s>\n","[CONT-LOG]",buffer);
      (void) fflush(stdout);
      }
    }
  //check if container still alive
  dead_pid=waitpid(cont_pid,&status,WNOHANG);
  if (dead_pid==cont_pid) {
    //purging queue
    while ((num_bytes=read(master_fd,buffer,sizeof(buffer)-1))>0) {
      buffer[num_bytes]='\0';
      (void) fprintf(stdout,"%s <%s>\n","[CONT-LOG]",buffer);
      (void) fflush(stdout);
      }
    break;      //no container anymore
    }
  }
return status;

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	container "main" program                */
/*						*/
/************************************************/
static int container_main()

{
#define OPEP    PRG":container_main"
#define OLDROOT "/.old_root"

static char *envps[][10] = {
            {
            "PATH=/usr/bin:/usr/sbin:/bin:/sbin",
            "TERM=linux",
            "container=vzgot",
            NULL
            },
            {
            "PATH=/usr/bin:/usr/sbin:/bin:/sbin",
            "TERM=linux",
	    "SYSTEMD_LOG_TARGET=console",       // <-- ON FORCE LA CIBLE ICI !
	    "SYSTEMD_LOG_LEVEL=debug",          // Opérationnel au niveau env
            "container=vzgot", 
            NULL
            }
          };

static char *actions[][10] = {
            {
            "/bin/bash",
            NULL
            },
            {
            "/lib/systemd/systemd",
            "--global",
            NULL
            }
          };

int status;
int doit;
char chr;
int phase;
char *rootfs;
char old_root[PATH_MAX];
_Bool proceed;

status=0;
doit=0;         //0 -> /bin/bash; 1->systemd
chr=SYNC_ERR;
rootfs=DUMMY_ROOT;
(void) snprintf(old_root,sizeof(old_root),"%s%s",rootfs,OLDROOT);
phase=0;
proceed=true;
while (proceed==true) {
  (void) fprintf(stdout,"%s phase='%d', pid='%d'\n",OPEP,phase,getpid());
  switch (phase) {
    case  0     :       //waiting for the superviseur Go
      (void) fprintf(stdout,"--- %s ---\n","[CONTAINER]  waiting to start");
      chr=read_pipe(CON_PIPE);
      switch (chr) {
        case SYNC_START :       //everything fine
          break;
        case SYNC_ERR   :
          (void) fprintf(stderr,"%s no sync_pipe (Abort!)\n",OPEP);
          goto TOOBAD;
          break;
        default         :
          (void) fprintf(stderr,"%s Unexpected sync char='%c' (Bug?)\n",OPEP,chr);
          goto TOOBAD;
          break;
        }
      break;
    case  1     :       //Unshare cgroup
      (void) fprintf(stdout,"--- %s ---\n","[CONTAINER]  started");
      if (unshare(CLONE_NEWCGROUP)<0) { //now /proc/self/cgroup is 0::/
        (void) fprintf(stderr,"%s unable to unshare CGROUP (error=<%s>)\n",
                               OPEP,strerror(errno));
        goto TOOBAD;
        }
      break;
    case  2     :       //doing sys_mount
      if (do_sysmount()==false) {
        (void) fprintf(stderr,"%s Was unable to mount needed file system (%s)\n",
                              OPEP,"Aborting!");
        goto TOOBAD;
        }
      if (dev_now_ready()==false) {
        (void) fprintf(stderr,"%s Supervisor NOT ready (%s)\n",
                              OPEP,"Aborting!");
        goto TOOBAD;
        }
      break;
    case  3     :       //make rootfs private
      if (mount(NULL,"/",NULL,MS_REC|MS_PRIVATE,NULL)<0) {
        (void) fprintf(stderr,"%s unable to make '/' private (error=<%s>)\n",
                               OPEP,strerror(errno));
        goto TOOBAD;
        }
      break;
    case  4     :       //binding rootf to itself
      if (mount(rootfs,rootfs,NULL,MS_BIND|MS_REC, NULL)<0) {
        (void) fprintf(stderr,"%s unable to bind <%s> to itself (error=<%s>)\n",
                               OPEP,rootfs,strerror(errno));
        goto TOOBAD;
        }
      break;
    case  5     :       //creating the pivot directory
      if (do_dir(old_root,0700)<0) 
        goto TOOBAD;
      break;
    case  6     :       //doing root pivot
      if (pivot_root(rootfs,old_root)<0) {
        (void) fprintf(stderr,"%s unable to pivot directory <%s> to <%s> "
                              "(error=<%s>)\n",
                              OPEP,rootfs,old_root,strerror(errno));
        goto TOOBAD;
        }
      break;
    case  7     :       //moving to rootfs as '/'
      if (chdir("/")<0) {
        (void) fprintf(stderr,"%s unable to use <%s> as '/' (error=<%s>)\n",
                              OPEP,rootfs,strerror(errno));
        goto TOOBAD;
        }
      break;
    case  8     :       //detaching the 
      if (umount2(OLDROOT,MNT_DETACH)<0) {
        (void) fprintf(stderr,"%s unable to unmount <%s> (error=<%s>)\n",
                              OPEP,OLDROOT,strerror(errno));
        goto TOOBAD;
        }
      break;
    case  9     :       //removing the empty directory
      if (rmdir(OLDROOT)<0) {
        (void) fprintf(stderr,"%s unable to remove directory <%s> (error=<%s>)\n",
                              OPEP,OLDROOT,strerror(errno));
        goto TOOBAD;
        }  
      break;
    case 10     :       //mount system
      break;
    case 11     :       //set apparmor
      break;    //JMPDBG
      if (apply_apparmor_profile("vzgot-default")<0) {
        (void) fprintf(stderr,"%s Was unable set apparmor protection (%s)\n",
                              OPEP,"Aborting!");
        goto TOOBAD;
        }
      break;
    case 12     :       //Empty
      {
      setsid();

      int fd = open("/dev/console", O_RDWR);
      if (fd < 0) {
        perror("Erreur open /dev/console");
        }
      // 3. SE CURITÉ : Forcer l'attribution comme terminal de contrôle (Contrôle des TTY)
        ioctl(fd, TIOCSCTTY, 0);

// 4. Rediriger stdin, stdout, stderr vers ce PTY
        dup2(fd, STDIN_FILENO);  // 0
        dup2(fd, STDOUT_FILENO); // 1
        dup2(fd, STDERR_FILENO); // 2

// 5. Fermer le descripteur devenu redondant
        if (fd > 2) close(fd);
      }

      break;
    case 13     :       //Empty
      break;
    case 14     :       //container ready for action
      (void) fprintf(stdout,"--- %s ---\n",
                            "[CONTAINER]  Starting execve as PID 1");
      if (execve(actions[doit][0],actions[doit],envps[doit])<0) {
        (void) fprintf(stderr,"%s unable to excute <%s> (error=<%s>)\n",
                              OPEP,actions[doit][0],strerror(errno));
        goto TOOBAD;
        }
      break;
    TOOBAD      :
      status=phase+1;   //To report the exact probleme
                        //NO BREAK
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
(void) exit(status);

#undef OPEP
}

/*

*/
/************************************************/
/*						*/
/*	syspivit "main" program                 */
/*						*/
/************************************************/
int main(int argc,char **argv)

{
#define OPEP    PRG":main"

int status;
char chr;
static char slave_name[64];
int target_fd;
int master_fd;
int slave_fd;
int cont_pid;
struct clone_args cl_args;
int phase;
_Bool proceed;

status=0;
chr='\000';
target_fd=-1;
cont_pid=-1;
(void) memset(&cl_args,'\000',sizeof(cl_args));
phase=0;
proceed=true;
while (proceed==true) {
  //(void) fprintf(stdout,"%s phase='%d', pid='%d'\n",OPEP,phase,getpid());
  switch (phase) {
    case  0     :       //opening sync_pipe
      if (open_synchro()==false) {
        (void) fprintf(stderr,"%s unable to get container/supervisor pipe (%s)\n",
                              OPEP,"Aborting!");
        goto TOOBAD;
        }
      break;
    case  1     :       //preparing all cgroup data
      if (do_sys_cgroup(CONTNAME)==false) {
        (void) fprintf(stderr,"%s unable to prepare cgroup (%s)\n",
                              OPEP,"Aborting!");
        goto TOOBAD;
        break;
        }
      break;
    case  2     :       //preparing target_fd
      if (subcont[1]!=(char *)0) {      //always
        char ppath[PATH_MAX];

        (void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",SYSFS,subcont[1],CONTNAME);
        if ((target_fd=open(ppath,O_RDONLY|O_DIRECTORY))<0) {
          (void) fprintf(stderr,"%s unable to open target <%s> (error=<%s>)\n",
                                 OPEP,ppath,strerror(errno));
          goto TOOBAD;
          }
        }
      break;
    case  3     :       //opening PTY device
      if (openpty(&master_fd,&slave_fd,slave_name,NULL,NULL)<0) {
        (void) fprintf(stderr,"%s unable to open pty device (error=<%s>)\n",
                                 OPEP,strerror(errno));
        goto TOOBAD;
        }
      (void) fprintf(stdout,"--- %s : %s ---\n",
                            "[SUPERVISOR] PTY set as ",slave_name);
      break;
    case  4     :       //setting master_fd in non block mode
      if (fcntl(master_fd,F_SETFL,O_NONBLOCK)<0) {
        (void) fprintf(stderr,"%s Unable to setmaster_fs mode (error=<%s>)\n",
                                 OPEP,strerror(errno));
        goto TOOBAD;
        }
      break;
    case  5     :       //doing container cloning
      cl_args.flags=CLONE_NEWNS|CLONE_NEWPID|CLONE_NEWNET|\
                    CLONE_NEWIPC|CLONE_NEWUTS|CLONE_INTO_CGROUP;
      cl_args.exit_signal=SIGCHLD;
      cl_args.cgroup=(__u64)target_fd;
      (void) fprintf(stdout,"--- %s %s ---\n","[SUPERVISOR]",
                                             "Starting container (clone3)");
      if ((cont_pid=syscall(SYS_clone3,&cl_args,sizeof(cl_args)))<0) {
        (void) fprintf(stderr,"%s Unable to start container <%s> (error=<%s>)\n",
                                 OPEP,CONTNAME,strerror(errno));
        goto TOOBAD;
        }
      (void) close(target_fd);
      break;
    case  6     :       //check if we are now the container
      if (cont_pid==0) {
        (void) container_main();        //Container process
        //NEVER EVER comeback
        }
      (void) fprintf(stdout,"--- %s container <%s> started, pid=%d ---\n",
                            "[SUPERVISOR]",
                            CONTNAME,cont_pid);
      (void) fprintf(stdout,"%s JMPDBG Write to container!)\n",OPEP);
      if (write_pipe(CON_PIPE,SYNC_START)<0) {
        (void) fprintf(stderr,"%s Unable start container <%s> (error=<%s>)\n",
                                 OPEP,CONTNAME,strerror(errno));
        goto TOOBAD;
        }
      break;
    case  7     :       //wait for container acknwoledge
      (void) fprintf(stdout,"%s JMPDBG waiting READ from container!)\n",OPEP);
      chr=read_pipe(SUP_PIPE);
      switch (chr) {
        case SYNC_DEV_READY     :       //everything fine
          (void) fprintf(stderr,"%s JMP got SYNC_DEV_READY\n",OPEP);
          break;
        case SYNC_ERR           :
          (void) fprintf(stderr,"%s Trouble receive container ACK (Abort!)\n",OPEP);
          goto TOOBAD;
          break;
        default         :
          (void) fprintf(stderr,"%s Unexpected sync char='%c' (Bug?)\n",OPEP,chr);
          goto TOOBAD;
          break;
        }
      (void) fprintf(stdout,"%s JMPDBG got awnser from container!)\n",OPEP);
      break;
    case  8     :       //Binding /dev/pts/x and /proc/cont_pid/root/dev/console
      (void) fprintf(stdout,"%s JMPDBG Should do mount!)\n",OPEP);
      if (mounting_pty(slave_name,cont_pid)==false) {
        (void) fprintf(stderr,"%s Unable mount container <%s> console (Abort!)\n",
                                 OPEP,CONTNAME);
        goto TOOBAD;
        }
      break;
    case  9     :       //say the pty is ready
      sleep(2);
      if (write_pipe(CON_PIPE,SYNC_PTY_DONE)<0) {
        (void) fprintf(stderr,"%s Unable start container <%s> (error=<%s>)\n",
                                 OPEP,CONTNAME,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 10     :       //check if we are now the container
      (void) fprintf(stdout,"--- %s container %s pid=%d should be up ---\n",
                            "[SUPERVISOR]",CONTNAME,cont_pid);
      (void) check_container(cont_pid,master_fd);
      (void) fprintf(stdout,"--- %s container %s contact lost ---\n",
                            "[SUPERVISOR]",CONTNAME);
      (void) close(master_fd);
      break;
    case 11     :       //properly unset Cgroup
      if (undo_sys_cgroup(CONTNAME)==false) {
        (void) fprintf(stderr,"%s Unable to properly release "
                              "container <%s> cgroup (error=<%s>)\n",
                              OPEP,CONTNAME,strerror(errno));
        goto TOOBAD;
        }
      break;
    TOOBAD      :
      status=phase+1;   //trouble trouble
                        //NO BREAK
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return status;

#undef OPEP
}
