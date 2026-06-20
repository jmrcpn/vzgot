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
/*	UNICNT:					*/
/*						*/
/************************************************/
#include        <sys/epoll.h>
#include        <sys/mount.h>
#include        <sys/sendfile.h>
#include        <sys/syscall.h>
#include        <sys/sysinfo.h>
#include        <sys/time.h>
#include        <sys/wait.h>
#include        <sys/stat.h>
#include	<ctype.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<limits.h>
#include	<math.h>
#include	<sched.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<syslog.h>
#include	<time.h>
#include	<unistd.h>

#include	"lowtyp.h"
#include	"dbglog.h"
#include	"utlapl.h"
#include	"utlsys.h"
#include	"subcfg.h"
#include	"subprc.h"
#include	"unicnt.h"

/*container filesystem				*/
#define	CLONPID	"first.pid"	/*cont process 1*/
#define	MTAB	"/etc/mtab"	/*mtab file	*/

#define	DDEV	"/dev"		/*system /dev	*/
#define	DEVFS	"devtmpfs"	/*/dev systype	*/

/*systemd sugget to have /proc/ and /sys to be	*/
/*mounted read only (MS_RDONLY).		*/
#define	MNTMODE	0		/*Mounting mode	*/


//network statistic index value
typedef	enum	{	//network statistic channel
	u_host,		//HOST channel
	u_cont,		//Container channel
	u_undef		//sentinel
	}CANTYP;
	
//swap information set by setmeminfo
static	u_vlong	swap_total_kb;
static	u_vlong	swap_free_kb;

//Container privileged working mode
PUBLIC	_Bool	privileged=false;

/*

*/
/************************************************/
/*						*/
/*	merging 2 strings			*/
/*						*/
/************************************************/
static char *merge_str(const char *forme,char *str1,char *str2)

{
char *merged;
int taille;

taille=strlen(forme)+strlen(str1)+strlen(str2)+3;
merged=(char *)calloc(taille,sizeof(char));
(void) snprintf(merged,taille,forme,str1,str2);
return merged;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to skip a processor defintion	*/
/*	from the HOST cpuinfo			*/
/*	file /proc/cpuinfo			*/
/*						*/
/************************************************/
static _Bool skipcpuinfo(FILE *fin)

{
_Bool isok;
char line[200];

isok=false;
while (fgets(line,sizeof(line)-1,fin)!=(char *)0) {
  if (line[0]=='\n') {	//we found the last line
    isok=true; 		//of one CPU description
    break;
    }
  }
return isok;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to duplicate part of HOST	*/
/*	file /proc/cpuinfo			*/
/*						*/
/************************************************/
static _Bool duplicate(FILE *fout,FILE *fin,long selected,long numcpu)

{
#define	OPEP	"unicnt.c:duplicate"
#define	MK(s,a) {s,sizeof(s)-1,a}

typedef enum {
	CORES,		//cpu cores field
	MODEL,		//model, line to change
	PROCESSOR,	//processor number line
	SIBLING,	//sibling field
	KEEP,		//cpu line we want to keep
	DISCARD		//cpu line to explicitly discard
	}action_t;

static struct {
	const char *key;
	size_t taille;
	action_t action;
	}markers[]={	//lenght order!
		MK("power management",DISCARD),
		MK("cache_alignment",KEEP),
		MK("initial apicid",DISCARD),
		MK("adresses sizes",KEEP),
		MK("fpu_exception",KEEP),
		MK("physical id",KEEP),
		MK("model name",MODEL),
		MK("cache size",KEEP),
		MK("cpu family",KEEP),
		MK("cpu cores",CORES),
		MK("processor",PROCESSOR),
		MK("vendor_id",KEEP),
		MK("bogomips",KEEP),
		MK("siblings",SIBLING),
		MK("core id",KEEP),
		MK("cpu MHz",KEEP),
		MK("clflush",KEEP),
		MK("apicid",DISCARD),
		MK("model",KEEP),
		MK("flags",KEEP),
		MK("fpu",KEEP),
		MK("wp",KEEP),
		{(char *)0,0,DISCARD}
		};

_Bool isok;
char line[900];
char buffer[1024];

isok=false;
(void) memset(line,'\000',sizeof(line));
while (fgets(line,sizeof(line)-1,fin)!=(char *)0) {
  char *sep;
  int i;

  (void) apl_cleanstring(line);
  if (line[0]=='\000') {
    isok=true; 	//found end of precessor description par
    break;
    }
  if ((sep=strchr(line,':'))==(char *)0) {
    (void) log_alert(0,"%s cpuinfo line=<%s> unexpected!",OPEP,line);
    break;	//forget everything
    }
  action_t act=DISCARD;
  i=0;
  while (markers[i].key!=(char *)0) {
    if (strncmp(line,markers[i].key,markers[i].taille)==0) {
      act=markers[i].action;
      break;
      }
    i++;
    }
  *sep='\000';		//cut the line at marker
  sep++;
  switch (act) {
    case KEEP		:
      (void) fprintf(fout,"%s:%s\n",line,sep);
      break;
    case PROCESSOR	:	
      (void) fprintf(fout,"%s: %ld\n",line,numcpu);
      break;
    case MODEL		:	
      (void) snprintf(buffer,sizeof(buffer),"%s:%s %s",
					line,sep,"[VZGOT Provisioned Core]");
      (void) fprintf(fout,"%s\n",buffer);
      break;
    case SIBLING	:	//NO BREAK
    case CORES		:	
      (void) fprintf(fout,"%s: %ld\n",line,selected);
      break;
    default	:	//NO BREAK!
      (void) log_alert(0,"%s CPU entry=<%s> not managed (Bug!!)",
			  OPEP,markers[i].key);
    case DISCARD:	
      //we do not care	
      break;
    }
  }
(void) fprintf(fout,"\n");
return isok;

#undef	OPEP
}
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
/*	procedure to bind /etc/vzgot/'target'	*/
/*	to /proc/'target' within the container.	*/
/*						*/
/************************************************/
_Bool do_binding(pid_t contpid,const char *target)

{
#define OPEP    "unicnt.c:do_binding,"

_Bool isok;
pid_t child;
int status;
int fd;
char ppath[200];
char dpath[200];
int phase;
_Bool proceed;

isok=false;
child=(pid_t)0;
status=10;
fd=0;
(void)snprintf(ppath,sizeof(ppath),"/proc/%d/ns/mnt",contpid);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //Forking process before switching to namespace
      child=fork();
      switch (child) {
        case -1 :       //Major malfunction
          (void) log_alert(0,"%s Unable to fork (error=<%s>, system? bug?)",
                              OPEP,strerror(errno));
          phase=999;
          break;
        case 0  :       //We are the child (let's work)
          break;
        default :       //we are still in the main process
          if (waitpid(child,&status,0)<0) {
            (void) log_alert(0,"%s waitpid problem (error=<%s>, system? bug?)",
                                OPEP,strerror(errno));
	    }
	  phase=999;	//going to main process exit
          break;
        }
      break;
    //child only area
    case 1      :       //trying to open namespace
      if ((fd=open(ppath,O_RDONLY))<0) {
        (void) log_alert(0,"%s unable to open <%s> (error=<%s>)",
                                OPEP,ppath,strerror(errno));
        (void) _exit(phase);
        }
      break;
    case 2      :       //deep dive in namespace
      if (setns(fd,CLONE_NEWNS)<0) {
        (void) log_alert(0,"%s unable to jmp to namespace (error=<%s>)",
                                OPEP,strerror(errno));
        (void) _exit(phase);
        }
      (void) close(fd);
      break;
    case 3      :       //binding the container to target depot
      (void) snprintf(ppath,sizeof(ppath),"/etc/%s/%s",VZGOT,target);
      (void) snprintf(dpath,sizeof(dpath),"/proc/%s",target);
      status=MS_BIND|MS_REC;
      if (mount(ppath,dpath,(const char *)0,status,(const void *)0)<0) {
        (void) log_alert(0,"%s unable to mount <%s> (error=<%s>)",
                            OPEP,dpath,strerror(errno));
        (void) _exit(phase);
        }
      break;
    case 4      :       // Set the mount propagation to shared.
                        // This is needed if a chroot is performed within
                        // the container, ensuring the container's /proc
                        // changes propagate into the chroot's /proc.
      status=MS_REC|MS_SHARED;
      if (mount((const char *)0,dpath,(const char *)0,status,(const void *)0)<0) {
        (void) log_alert(0,"%s unable to set the mount <%s> shared (error=<%s>)",
                            OPEP,dpath,strerror(errno));
        (void) _exit(phase);
        }
      break;
    case 5      :       // Remount the bind point as read-only.
                        // Note: MS_BIND is required alongside MS_REMOUNT 
                        // to target only the bind mount, not the whole VFS.
      status=MS_BIND|MS_REMOUNT|MS_RDONLY;
      if (mount(ppath,dpath,(const char *)0,status,(const void *)0)<0) {
        (void) log_alert(0,"%s unable to re-mount <%s> readonly (error=<%s>)",
                            OPEP,dpath,strerror(errno));
        _exit(phase);
        }
      break;
    case 6      :       //everything is fine child is due to vanish
      (void) _exit(0);
      break;
    default     :       //SAFE Guard
      if (WIFEXITED(status)&&(WEXITSTATUS(status)==0))
	isok=true;
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
/*	procedure to decide if a binding is	*/
/*	acceptable or not.			*/
/*						*/
/************************************************/
static _Bool check_binding(pid_t contpid,const char *target)

{
#define	OPEP	"unicnt.c:check_binding"

#define	MXTRY	10
#define	TRYGAP  60*30	//MXTRY reconnect within 30 minutes tolerance

static time_t last[MXTRY];
static int count;

_Bool isok;
int fd;
char fname[200];
time_t curtime;
int phase;
_Bool proceed;

isok=false;
fd=0;
(void) snprintf(fname,sizeof(fname),"/proc/%d/root/proc/%s",contpid,target);
curtime=time((time_t *)0);
phase=0;
proceed=true;
while (proceed==true) {
//  (void) log_alert(0,"%s JMPDBG phase='%d' isok='%d' target=<%s>",
//		      OPEP,phase,isok,target);
  switch (phase) {
    case 0	:	//checking if binding up
      if ((fd=open(fname,O_WRONLY)<0)) {
	switch (errno) {
	  case EROFS	:	//everything fine
	    isok=true;		//expected stat
	    phase=999;		//no need to go further
	    break;
	  default	:
	    (void) log_alert(0,"%s binding unexpected status <%s> (bug?)",
				OPEP,strerror(errno));
	    break;
	  }
	}
      else  
        (void) close(fd);
      break;
    case 1	:	//purging count
      while ((count>0)&&(last[0]<=(curtime-TRYGAP))) {
	count--;
	if (count>0) 
	  (void)memmove(last,last+1,sizeof(time_t)*count);
	last[count]=0;
	}
      if (count<(MXTRY-1))
	phase++;	//no need to check time AND try
      break;
    case 2	:	//maximun try reach within TRYGAP
      if (last[0]>(curtime-TRYGAP)) {
	(void) log_alert(0,"%s, too many binding on container '%d'",OPEP,contpid);
	(void) log_alert(0,"%s, container '%d' (major malfunction)!",OPEP,contpid);
	phase=999;
	}
      break;
    case 3	:	//lets do binding
      if ((isok=do_binding(contpid,target))==false) {
	(void) log_alert(0,"%s, binding on container '%d' not successful (System?)",
			    OPEP,contpid);
	phase=999;	//
	}
      break;
    case 4	:	//take note of this last binding request
      last[count]=curtime;
      count++;
      (void) log_alert(0,"%s, set '%d' container binding (isok='%d' count='%d')!",
			  OPEP,contpid,isok,count);
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
/*	Returning the container path.		*/
/*	contpath array is dynamically assigned	*/
/*	must be freed when not needed anymore.	*/
/*						*/
/************************************************/
static char *getcontpath(const char *contname)

{
char *homefs;
char *contpath;

homefs=getenv(HOMEFS);
contpath=(char *)0;
if (homefs==(char *)0) {
  char *vzpath;

  vzpath=apl_appdir(d_vzgot);
  contpath=merge_str("%s/%s",vzpath,(char *)contname);
  vzpath=apl_freestr(vzpath);
  }
else
  contpath=strdup(homefs);
return contpath;
}
/*

*/
/************************************************/
/*						*/
/*	procedure to feed the load average to	*/
/*	container speciel feed file		*/
/*	/etc/vzgot/loadavg			*/
/*						*/
/************************************************/
static _Bool setloadavg(STATYP *contstat,const char *target)

{
#define	OPEP	"unicnt.c:setloadavg,"

_Bool isok;
char *contpath;
char ppath[PATH_MAX];
FILE *fichier;
int phase;
_Bool proceed;

isok=false;
contpath=getcontpath(contstat->contname);
(void) snprintf(ppath,sizeof(ppath),"%s/rootfs/etc/%s/%s",contpath,VZGOT,target);
fichier=(FILE *)0;
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d' delta_t='%lf'",OPEP,phase,delta_t);
  switch (phase) {
    case 0	:	//feeding
      if ((fichier=fopen(ppath,"r+"))==(FILE *)0) {
	if (errno==ENOENT) {
          (void) log_alert(0,"%s lets create file <%s>",OPEP,ppath);
	  fichier=fopen(ppath,"w+");
	  }
        }
      if (fichier==(FILE *)0) {
        (void) log_alert(0,"%s Unable to open file <%s> (error=<%s>)",
                            OPEP,ppath,strerror(errno));
	phase=999;	//potential trouble
	}
      break;
    case 1	:	//inserting loadavg within file (test purpose)
      (void) rewind(fichier);
      (void) fprintf(fichier,"%s\n",contstat->loadavg);
      (void) fflush(fichier);
      if (ftruncate(fileno(fichier),ftell(fichier))<0) {
        (void) log_alert(0,"%s Unable to truncate file <%s> (error=<%s>)",
                            OPEP,ppath,strerror(errno));
	phase=999;	//potential trouble
	}
      (void) fclose(fichier);
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
contpath=apl_freestr(contpath);
return isok;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to feed the load memory info	*/
/*	container speciel feed file		*/
/*	/etc/vzgot/meminfo			*/
/*						*/
/************************************************/
static _Bool setmeminfo(const char *contname,const char *target)

{
#define	OPEP	"unicnt.c:setmeminfo,"

_Bool isok;
unsigned long long memmax;
unsigned long long memcur;
unsigned long long swapmax;
unsigned long long swapcur;
unsigned long long mem_total_kb;
unsigned long long mem_free_kb;
unsigned long long zero_kb;
struct sysinfo info;
FILE *fmem;
char *contpath;
char fname[200];
int phase;
_Bool proceed;

isok=false;
memmax=(unsigned long long)0;
memcur=(unsigned long long)0;
swapmax=(unsigned long long)0;
swapcur=(unsigned long long)0;
mem_total_kb=(unsigned long long)0;
mem_free_kb=(unsigned long long)0;
swap_total_kb=(unsigned long long)0;
swap_free_kb=(unsigned long long)0;
zero_kb=(unsigned long long)0;
(void) memset(&info,'\000',sizeof(info));
fmem=(FILE *)0;
contpath=getcontpath(contname);
(void) snprintf(fname,sizeof(fname),"%s/rootfs/etc/%s/%s",contpath,VZGOT,target);
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d'",OPEP,phase);
  switch (phase) {
    case 0	:	//extracting default value
      if (sysinfo(&info)<0) {
	(void) log_alert(0,"%s Unable to get kernel sysinfo (error=<%s> system?)",
			    OPEP,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//setting the system value
      memmax=((unsigned long long)info.totalram*info.mem_unit);
      memcur=memmax-((unsigned long long)info.freeram*info.mem_unit);
      swapmax=((unsigned long long)info.totalswap*info.mem_unit);
      swapcur=swapmax-((unsigned long long)info.freeswap*info.mem_unit);
      break;
    case 2	:	//reading the current cgroup memory.max
      if (prc_getcgroupmem(contname,"memory.max",&memmax)==false) {
        (void) log_alert(0,"%s Unable to get container maximun memory (Config?)",
			    OPEP);
	phase=999;	//trouble trouble
	}
      break;
    case 3	:	//getting the memory.max value
      if (prc_getcgroupmem(contname,"memory.current",&memcur)==false) {
        (void) log_alert(0,"%s Unable to get container current memory (Config?)",
			    OPEP);
	phase=999;	//trouble trouble
	}
      break;
    case 4	:	//reading the current cgroup memory.max
      if (prc_getcgroupmem(contname,"memory.swap.max",&swapmax)==false) {
        (void) log_alert(0,"%s Unable to get container maximun swap (Config?)",
			    OPEP);
	phase=999;	//trouble trouble
	}
      break;
    case 5	:	//getting the memory.max value
      if (prc_getcgroupmem(contname,"memory.swap.current",&swapcur)==false) {
        (void) log_alert(0,"%s Unable to get container current swap (Config?)",
			    OPEP);
	phase=999;	//trouble trouble
	}
      break;
    case 6	:	//check if memory value are sane
      if (memmax<memcur) {
        (void) log_alert(0,"%s mem: max='%llu' smaller than cur='%llu' (Config?)",
			    OPEP,memmax,memcur);
	memcur=memmax;	//capping value
	}
      if (swapmax<swapcur) {
        (void) log_alert(0,"%s swap: max='%llu' smaller than cur='%llu' (Config?)",
			    OPEP,swapmax,swapcur);
	swapcur=swapmax;	//capping value
	}
      mem_total_kb=memmax/1024; 
      mem_free_kb=(memmax-memcur)/1024;
      swap_total_kb=swapmax/1024; 
      swap_free_kb=(swapmax-swapcur)/1024;
      break;
    case 7	:	//check if memory value are sane
      if ((fmem=fopen(fname,"w"))==(FILE *)0) {
        (void) log_alert(0,"%s Unable to open <%s> (error=<%s> config?)",
			    OPEP,fname,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 8	:	//writing file
      (void) fprintf(fmem,"MemTotal:       %14llu kB\n",mem_total_kb);
      (void) fprintf(fmem,"MemFree:        %14llu kB\n",mem_free_kb);
      (void) fprintf(fmem,"MemAvailable:   %14llu kB\n",mem_free_kb);
      (void) fprintf(fmem,"Buffer:         %14llu kB\n",zero_kb);
      (void) fprintf(fmem,"Cached:         %14llu kB\n",zero_kb);
      (void) fprintf(fmem,"SwapTotal:      %14llu kB\n",swap_total_kb);
      (void) fprintf(fmem,"SwapFree:       %14llu kB\n",swap_free_kb);
      (void) fclose(fmem);
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
contpath=apl_freestr(contpath);
return isok;

#undef	OPEP

}
/*

*/
/************************************************/
/*						*/
/*	procedure to generate a special swaps 	*/
/*	file.					*/
/*						*/
/************************************************/
static _Bool setswapsinfo(const char *contname,const char *target)

{
#define	OPEP	"unicnt.c:setswapsinfo"

_Bool isok;
FILE *fichier;
char *contpath;
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=false;
fichier=(FILE *)0;
contpath=getcontpath(contname);
(void) snprintf(ppath,sizeof(ppath),"%s/rootfs/etc/%s/%s",contpath,VZGOT,target);
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d'",OPEP,phase);
  switch (phase) {
    case 0	:	//opening the swap file
      if ((fichier=fopen(ppath,"w"))==(FILE *)0) {
        (void) log_alert(0,"%s Unable to open <%s> (error=<%s> config?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//writing data
      (void) fprintf(fichier,"Filename\t\t\t\tType\t\tSize\t\tUsed\t\tPriority\n");
      (void) fprintf(fichier,"/dev/%s\t\t\t\thosted\t\t%-10llu\t%-10llu\t-2\n",
			     "cvz-swap",swap_total_kb,swap_total_kb-swap_free_kb);
      (void) fclose(fichier);
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
contpath=apl_freestr(contpath);
return isok;

#undef OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to generate a special file	*/
/*	cpuinfo will override the container	*/
/*	(HOST) value. Regeneration occur at	*/
/*	start and after only if the number of	*/
/*	was changed via cpu.mac at host		*/
/*						*/
/************************************************/
static _Bool setcpuinfo(const char *contname,const char *target)

{
#define	OPEP	"unicnt.c:setcpuinfo"
#define	CPUINFO	"/proc/cpuinfo"

_Bool isok;
FILE *fin;
FILE *fout;
char *contpath;
char ppath[PATH_MAX];
PHYCPU hostcpus;
PHYCPU contcpus;
int numcpu;
int phase;
_Bool proceed;

isok=false;
fin=(FILE *)0;
fout=(FILE *)0;
contpath=getcontpath(contname);
(void) snprintf(ppath,sizeof(ppath),"%s/rootfs/etc/%s/%s",contpath,VZGOT,target);
numcpu=0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//get the hardware cpulist
      if (sys_get_cpu_host_list(&hostcpus)==false) {
        (void) log_alert(0,"%s Unable to get HOST cpus list (Aborting!)",OPEP);
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//get the container list
      if (sys_get_cpu_cont_list(&contcpus)==false) {
        (void) log_alert(0,"%s Container cpus list missing (Aborting!)",OPEP);
	phase=999;	//trouble trouble
	}
      break;
    case 2	:	//opening the container cpuinfo
      if ((fout=fopen(ppath,"w"))==(FILE *)0) {
        (void) log_alert(0,"%s Unable to open <%s> (error=<%s> config?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 3	:	//opening the HOST /proc/cpuinfo
      if ((fin=fopen(CPUINFO,"r"))==(FILE *)0) {
        (void) log_alert(0,"%s Unable to open <%s> (error=<%s> config?)",
			    OPEP,CPUINFO,strerror(errno));
	(void) fclose(fout);
	phase=999;	//trouble trouble
	}
      break;
    case 4	:	//get the current CPU number
      for (int i=0;i<hostcpus.maxcpu;i++) {
	if (CPU_ISSET(i,&(hostcpus.allocated))==0)
	  continue;	//cpu not available to host
	if (CPU_ISSET(i,&(contcpus.allocated))==0) {
	  (void) skipcpuinfo(fin);
	  continue;	//cpu not available to container
	  }
	(void) duplicate(fout,fin,contcpus.selected,numcpu);
	numcpu++;
	}
      (void) fclose(fout);
      (void) fclose(fin);
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
contpath=apl_freestr(contpath);
return isok;

#undef	CPUINFO
#undef 	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to update container pseudo	*/
/*	/proc in due time			*/
/*	Return true if update went without flaw	*/
/*	false otherwise.			*/
/*						*/
/************************************************/
static _Bool updateproc(STATYP *contstat)

{
#define	OPEP	"unicnt.c:updateproc"
#define	PACE	5.0		//number of second pace update

static TIMETIC last_time={0,0};

static struct target {
	const char *val;
	int cas;
	}targets[]={
		{"loadavg",1},	//Generating cpu loadaverage
		{"meminfo",2},	//Generating meminfo load
		{"swaps",3},	//Generating swap information
		{"cpuinfo",4},	//Generating cpu information information
		{(const char *)0,0}
		};

_Bool isok;
double delta_t;
struct target *ptr;
TIMETIC cur_time;
int phase;
_Bool proceed;

isok=true;
delta_t=(double)0.0;
ptr=targets;
cur_time.tv_sec=0;
cur_time.tv_nsec=0;
phase=0;
proceed=true;
while (proceed==true) {
//  (void) log_alert(0,"%s JMPDBG phase='%d' isok='%d' delta_t='%lf'",
//		      OPEP,phase,isok,delta_t);
  switch (phase) {

    case 0	:	//getting monotonic time;
      if (clock_gettime(CLOCK_MONOTONIC,&cur_time)<0) {
        (void) log_alert(0,"%s Unable to get MONOTONIC clock! (error=<%s> system?)",
			    OPEP,strerror(errno));
	isok=false;	//Big trouble
	phase=999;
	}
      break;
    case 1	:	//computing time delta
      delta_t=(double)(cur_time.tv_sec-last_time.tv_sec)+
              (double)((cur_time.tv_nsec-last_time.tv_nsec)/1e9);
      if (delta_t<PACE)	//is time to update /proc counter
	phase=999;	//No!; no need to update
      break;
    case 2	:	//updating status
      if (sys_update_cont_status(contstat)==false) {
        (void) log_alert(0,"%s No container up to date (system?)",OPEP);
	phase=999;
	}
      break;
    case 3	:	//doing infos update
      ptr=targets;
      while (ptr->val!=(char *)0) {
	_Bool isset;

	isset=false;
	switch (ptr->cas) {
	  case 1	:	// computing loadavg
	    isset=setloadavg(contstat,ptr->val);
	    break;
	  case 2	:	// computing memeinfo
	    isset=setmeminfo(contstat->contname,ptr->val);
	    break;
	  case 3	:	// generating swaps information
	    isset=setswapsinfo(contstat->contname,ptr->val);
	    break;
	  case 4	:	// generating swaps information
	    isset=setcpuinfo(contstat->contname,ptr->val);
	    break;
	  default	:	//unexpected case report
            (void) log_alert(0,"%s Unexpected <%s> target! (Bug?)",OPEP,ptr->val);
	    isset=false;
	    break;
	  }
	if (isset==true)
	  isok&=check_binding(contstat->contpid,ptr->val);
	ptr++;
	}
      last_time=cur_time; 
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Checking and Waiting to be ready to	*/
/*	accept user mapping.			*/
/*	Return true if mapping possible, or	*/
/*	false if trouble or timeout.		*/
/*						*/
/************************************************/
static _Bool iscloneready(pid_t clonepid,char *map)

{
#define OPEP    "unicnt.c:iscloneready"
#define	SLICE	10000	//10 Millisecond
#define MXSEC	5       //maximun seconds waiting time
			//for CLONE_NEWUSER to be actif
			//within clone process
#define	MXWAIT	((MXSEC*1000000)/SLICE)

_Bool done;

done=false;
for (int i=0;i<MXWAIT;i++) {
  int handle;
  int status;
  int errloc;
  char mode[200];

  (void) usleep(SLICE);
  (void) memset(mode,'\000',sizeof(mode));
  if ((handle=open(map,O_RDONLY))<0) {
    (void) log_alert(0,"%s Unable to open file <%s> (error=<%s>)",
                        OPEP,map,strerror(errno));
    break;      //no need to try anymore!
    }
  status=read(handle,mode,sizeof(mode)-1);
  errloc=errno;
  (void) close(handle);
  if (status<0) {
    (void) log_alert(0,"%s Unable to read file <%s> (error=<%s>)",
                        OPEP,map,strerror(errloc));
    break;      //no need to try anymore!
    }
  if (strlen(mode)==0) {
    (void) log_alert(2,"%s clone process '%d' detected in CLONE_NEWUSER mode",
                        OPEP,clonepid);
    done=true;
    break;      //check successful
    }
  if ((i%(MXWAIT/MXSEC))==0) {
    (void) log_alert(0,"%s Waiting (%2d/%2d) for process '%d' to "
                       "be in CLONE_NEWUSER mode",OPEP,i,MXWAIT,clonepid);
    }
  if (kill(clonepid,0)<0) {
    (void) log_alert(0,"%s clone process'%d' Premature exit (bug?)",
		    	OPEP,clonepid);
    break;
    }
  }
return done;

#undef	MXSEC
#undef  SLICE
#undef  MXWAIT
#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to set the container system	*/
/*	directory				*/
/*						*/
/************************************************/
static _Bool do_sysmount(char *rootfs)

{
#define OPEP    "vzgot:do_sysmount"

_Bool done;
int options;
char ppath[50];
int phase;
_Bool proceed;

done=false;
options=0;
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(2,"%s phase='%d'",OPEP,phase);
  switch (phase) {
    case 0      :       //Empty
      break;
    case 1      :       //Empty
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",rootfs,"proc");
      if (mount("proc",ppath,"proc",MS_NODEV|MS_NOSUID|MS_NOEXEC,"")<0)  {
        (void) log_alert(0,"%s Unable to mount <%s> within container "
                           "(error=<%s>)",OPEP,ppath,strerror(errno));
        phase=999;          //trouble trouble
        }
      break;
    case 2      :       // /proc/sys/net
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",rootfs,"proc/sys/net");
      if (mount("none",ppath,"proc",MS_NODEV|MS_NOSUID|MS_NOEXEC,"")<0)  {
        (void) log_alert(0,"%s Unable to mount <%s> within container "
                           "(error=<%s>)",OPEP,ppath,strerror(errno));
        phase=999;          //trouble trouble
        }
      break;
    case 3      :       //mounting /proc/sys
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",rootfs,"proc/sys");
      if (mount(ppath,ppath,(char *)0,MS_BIND,(char *)0)<0)  {
        (void) log_alert(0,"%s Unable to bind mount <%s> within container "
                           "(error=<%s>)",OPEP,ppath,strerror(errno));
        phase=999;          //trouble trouble
        }
      break;
    case 4	:
      options=MS_REMOUNT|MS_RDONLY|MS_BIND;
      if (mount((char *)0,ppath,(char *)0,options,(char *)0) < 0) {
        (void) log_alert(0, "%s Unable to remount <%s> as read-only (error=<%s>)",
			      OPEP, ppath, strerror(errno));
        phase = 999;
        }
      break;
    case 5      :       //mounting sys
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",rootfs,"sys");
      options=MS_NODEV|MS_NOSUID|MS_NOEXEC|MS_RDONLY;
      if (mount("sysfs",ppath,"sysfs",options,"")<0)  {
        (void) log_alert(0,"%s Unable to mount <%s> within container "
                           "(error=<%s>)",OPEP,ppath,strerror(errno));
        phase=999;          //trouble trouble
        }
      break;
    case 6      :       // FORCE CGROUP2 RELATIVE VIEW
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",rootfs,"sys/fs/cgroup");
      // Note : On ne met pas MS_RDONLY ici pour laisser le
      // container gérer ses sous-cgroups c1, c2
      options=MS_NODEV|MS_NOSUID|MS_NOEXEC; 
      if (mount("cgroup2",ppath,"cgroup2",options,"")<0)  {
        (void) log_alert(0,"%s Unable to force cgroup2 <%s> within container "
                            "(error=<%s>)",OPEP,ppath,strerror(errno));
        phase=999;          //trouble trouble
        }
      break;
    case 7      :       //Empty
      done=true;
      break;
    default     :       //SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return done;

#undef  OPEP
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
PUBLIC const char *cal_loadavg(const char *contname,uint16_t nbr_cpu,double delta_t)

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
uint32_t last_pid;
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
last_pid=0;
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
      if (prc_get_last_pid(contname,&last_pid)==false) {
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

*/
/************************************************/
/*						*/
/*	Procedure to check the current uid and	*/
/*	gid to be "right"			*/
/*	Wait at max for delay second.		*/
/*						*/
/************************************************/
PUBLIC _Bool cnt_wait_goodid(int delay,uid_t uid,gid_t gid)

{
#define	SLICE	10000	//waiting time between check

_Bool done;

done=false;
delay*=(1000000/SLICE);
for (int i=0;i<delay;i++) {
  (void) usleep(SLICE);
  if ((getuid()==uid)&&(getgid()==gid)) {
    done=true;
    break;
    }
  }
return done;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to set the container working	*/
/*	"root" user at kernel level, used by	*/
/*	container with CLONE_NEWUSER mode.	*/
/*						*/
/************************************************/
PUBLIC _Bool cnt_mapcontids(pid_t clonepid,uid_t contuid,gid_t contgid)

{
#define OPEP    "unicnt.c:cnt_mapcontids"
#define MXMAP   sizeof(names)/sizeof(char *)

static char *names[]={
        "setgroups",
        "uid_map",
        "gid_map"
        };


_Bool done;
char locnames[MXMAP][100];
char data[MXMAP][100];
int phase;
_Bool proceed;

done=false;
(void) memset(locnames,'\000',sizeof(locnames));
(void) memset(data,'\000',sizeof(data));
phase=0;
proceed=true;
while (proceed==true) {
  (void) log_alert(3,"%s phase='%d'",OPEP,phase);
  switch (phase) {
    case 0        :     //preparing file name
      for (int i=0;i<MXMAP;i++) 
        (void) snprintf(locnames[i],sizeof(locnames[i]),
                         "/proc/%d/%s",clonepid,names[i]);
      break;
    case 1        :     //waiting for clone process to be in CLONE_NEWUSER 
      if (iscloneready(clonepid,locnames[1])==false) {
        (void) log_alert(0,"%s Unable to map user in cloned "
                         "process='%d' (aborting!)",OPEP,clonepid);
        if (kill(clonepid,0)==0) {
          (void) log_alert(0,"%s terminating unresponsive clone process (pid=%d)",
                            OPEP,clonepid);
          (void) kill(clonepid,SIGTERM);
          (void) sleep(2);
          if (kill(clonepid,0)==0) {
            (void) log_alert(0,"%s Overkill unresponsive clone process (pid=%d)",
                            OPEP,clonepid);
            (void) kill(clonepid,SIGKILL);
            }
          }
        phase=999;      //no need to go further
        }
      break;
    case 2        :     //preparing data
      (void) snprintf(data[0],sizeof(data[0]),"allow");
      (void) snprintf(data[1],sizeof(data[1]),"0 %d %d",contuid,65536);
      (void) snprintf(data[2],sizeof(data[2]),"0 %d %d",contgid,65536);
      break;
    case 3        :     //storing data in process map
      for (int i=0;i<MXMAP;i++) {
        int handle;

        if ((handle=open(locnames[i],O_WRONLY))<0) {
	  (void) log_alert(0,"%s, unable to open <%s> (error=<%s>)",
                            OPEP,locnames[i],strerror(errno));
          phase=999;
          break;        //Trouble no need to go further
          }
        if (write(handle,data[i],strlen(data[i]))<0) {
	  (void) log_alert(0,"%s, unable to write <%s> to <%s> (error=<%s>)",
                            OPEP,data[i],locnames[i],strerror(errno));
          phase=999;    //no good status
          }
        (void) close(handle);
        }
      break;
    case 4      :       //everyhing is right, lets report it
      done=true;
      break;
    default     :       //SAFE guard
      proceed=false;
      break;
    }
  phase++;
  }
return done;

#undef  MXMAP
#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure used by container (once	*/
/*	cloned) to make sure the current ID is	*/
/*	propperly set.				*/
/*						*/
/************************************************/
PUBLIC _Bool cnt_wait_for_mapuser(uid_t contuid,gid_t contgid)

{
#define OPEP    "unicnt.c:cnt_wait_for_mapuse"
#define	SLICE	10000	//10 Milli-seconde
#define MXSEC	2       //maximun secondes waiting time
			//for CLONE_NEWUSER to be actif
			//within clone process
#define	MXWAIT	((MXSEC*1000000)/SLICE)

_Bool done;

done=false;
for (int i=0;i<MXWAIT;i++) {
  (void) usleep(1000);
  if ((getuid()==contuid)&&(getgid()==contgid)) {
    done=true;
    break;
    }
  }
return done;

#undef	MXSEC
#undef  SLICE
#undef  MXWAIT
#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to pivot the container root	*/
/*	file system.				*/
/*						*/
/************************************************/
PUBLIC _Bool cnt_pivot(char *contname,int cloneflgs)

{
#define	OPEP	"unicnt.c:cnt_pivot,"
#define	HOSTFS	"hostfs_root"	//container host PIVOT

int done;
char *rootfs;
char *contpath;
unsigned long mntflags;
char ppath[200];
int phase;
int proceed;

done=false;
rootfs=(char *)0;
contpath=getcontpath(contname);
mntflags=(unsigned long)0;
rootfs=(char *)calloc(PATH_MAX,sizeof(char));
(void) snprintf(rootfs,PATH_MAX,"%s/%s",contpath,ROOTFS);
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(2,"%s phase='%d'",OPEP,phase);
  switch (phase) {
    case 0	:	//Ensure that 'new_root' and its parent mount don't have
			//shared propagation (which would cause pivot_root() to
              		//return an error), and prevent propagation of mount
			//events to the initial mount namespace.
      mntflags=MS_REC|MS_SLAVE;
      if (mount(NULL,"/",0,mntflags,NULL)<0) {
        (void) log_alert(0,"%s unable to stop shared propagation in container "
			   "(error=%s)",OPEP,strerror(errno));
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 1	:	//Ensure that 'rootfs' is a mount point.
      mntflags=MS_BIND;
      if (mount(rootfs,rootfs,NULL,mntflags,NULL)<0) {
        (void) log_alert(0,"%s unable to bind container <%s> directory "
			   "(error=%s)",
			   OPEP,rootfs,strerror(errno));
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 2	:	//Mounting system directory
      if (do_sysmount(rootfs)==false) {
        (void) log_alert(0,"%s unable to mount container <%s> system directory",
			    OPEP,contname);
	phase=999;	//trouble trouble
        }
      break;
    case 3	:	///Empty
      break;
    case 4	:	//creating the "pivot" repository
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",rootfs,HOSTFS);
      if (mkdir(ppath,0777)<0) {
	switch (errno) {
	  case EEXIST	:	//this could append (its fine)
	    (void) log_alert(0,"%s, pivoting <%s> directory  allready existing",
                               OPEP,ppath);
	    break;
	  default	:	//Unexpected result
	    (void) log_alert(0,"%s Unable to create pivoting <%s> directory "
                               "(error=<%s>)",OPEP,HOSTFS,strerror(errno));
            phase=999; 
	    break;
	  }
	}
      break;
    case 5	:	/*moving to rootfs	*/
      if (pivot_root(rootfs,ppath)<0) {
        (void) log_alert(0,"%s unable to do pivot to old root <%s> "
			   "to <%s> (error=<%s<)",
			   OPEP,rootfs,ppath,strerror(errno));
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 6	:	//chrooting to root directory
      if (chdir("/")<0) {
        (void) log_alert(0,"%s unable to reach container <%s> "
			   "root directory (error=%s)",
			   OPEP,contname,strerror(errno));
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 7	:	// Forcing /proc propagation to PRIVATE to 
			//completely isolate the container's VFS tree
			// from host exposure during internal rbinds.
      mntflags=MS_REC|MS_PRIVATE;
      if (mount((const char *)0,"/proc",(const char *)0,mntflags,NULL)<0) {
        (void) log_alert(0,"%s container <%s> %s (error=%s)",
                            OPEP,
			    contname,
        		    "unable to lock /proc propagation",
			    strerror(errno));
        phase=999;    /*trouble trouble    */
        }
      break;
    case 8	:	//empty
      break;
    case 9	:	/*mounting proc dir	*/
      break;
    case 10	:	//mounting DSYS if needed
      break;
    case 11	:	//empty
      if (umount2(HOSTFS,MNT_DETACH)<0) {
        (void) log_alert(0,"%s unable to unmount container <%s> "
			   "oldroot <%s> (error=%s)",
			   OPEP,contname,HOSTFS,strerror(errno));
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 12	:	/*mounting dev RD_ONLY?	*/
      if (rmdir(HOSTFS)<0) 
        (void) log_alert(0,"%s unable to remove <%s> directory "
			   "from container <%s>, (error=%s)",
			   OPEP,HOSTFS,contname,strerror(errno));
      break;
    case 13	:	/* everything fine	*/
      (void) fprintf(stdout,"%s chroot done for container <%s>\n",
			    apl_ascsystime(time((time_t *)0)),contname);
      (void) fflush(stdout);
      done=true;
      break;
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
contpath=apl_freestr(contpath);
rootfs=apl_freestr(rootfs);
return done;


#undef	HOSTFS
#undef	DPROC
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to set a file with the clone	*/
/*	ip number.				*/
/*						*/
/************************************************/
PUBLIC int cnt_setclonepid(char *contname,pid_t cpid)

{
#define	OPEP	"unicnt.c:cnt_setclonepid,"

int done;
FILE *fichier;
char *filename;
char *contpath;
int phase;
int proceed;

done=false;
fichier=(FILE *)0;
filename=(char *)0;
contpath=getcontpath(contname);
filename=merge_str("%s/%s",contpath,CLONPID);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	/*opening the file	*/
      if ((fichier=fopen(filename,"w"))==(FILE *)0) {
	(void) log_alert(0,"%s, Unable to open pidfile <%s> (error=<%s>)",
			    appname,filename,strerror(errno));
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 1	:	/*writing  PID in it	*/
      if (fprintf(fichier,"%d\n",cpid)<0) {
	(void) log_alert(0,"%s, Unable to write pidfile in container <%s> (error=<%s>)",
			    appname,contname,strerror(errno));
	phase=999;	/*trouble trouble	*/
	}
      (void) fclose(fichier);
      break;
    case 2	:	/*everything fine	*/
      done=true;
      break;
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
(void) apl_freestr(filename);
(void) apl_freestr(contpath);
return done;

#undef OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to get the clone pid stored	*/
/*	within the clonpid file			*/
/*						*/
/************************************************/
PUBLIC pid_t cnt_getclonepid(char *contname)

{
pid_t clonepid;
FILE *fichier;
char *filename;
char *contpath;

clonepid=(pid_t)0;
contpath=getcontpath(contname);
filename=merge_str("%s/%s",contpath,CLONPID);
if ((fichier=fopen(filename,"r"))!=(FILE *)0) {
  char strloc[80];

  if (fgets(strloc,sizeof(strloc)-1,fichier)!=(char *)0) {
    (void) sscanf(strloc,"%d",&clonepid);
    (void) fclose(fichier);
    }
  }
(void) apl_freestr(filename);
(void) apl_freestr(contpath);
return clonepid;
}
/*

*/
/************************************************/
/*						*/
/*	procedure to remove the file with the	*/
/*	clone pid.				*/
/*						*/
/************************************************/
PUBLIC int cnt_rmclonepid(char *contname)

{
int done;
char *filename;
char *contpath;

done=true;
filename=(char *)0;
contpath=getcontpath(contname);
filename=merge_str("%s/%s",contpath,CLONPID);
if (unlink(filename)<0) {
  (void) log_alert(0,"%s, Unable to remove pidfile in container <%s> (error=<%s>)",
		     appname,contname,strerror(errno));
  done=false;
  }
(void) apl_freestr(filename);
(void) apl_freestr(contpath);
return done;
}
/*

*/
/************************************************/
/*						*/
/*	procedure to execute a script		*/
/*						*/
/************************************************/
PUBLIC _Bool cnt_initscript(const char *scriptname,const char *fmt,...)

{
#define	OPEP	"unicnt.c:cnt_initscript"
#define	BSIZE	2048

_Bool done;
va_list args;
FILE *canal;
char *libexecpath;
char *parmlst;
char *cmd;
int phase;
int proceed;

done=false;
va_start(args,fmt);
canal=(FILE *)0;
libexecpath=apl_appdir(d_libexec);
parmlst=(char *)0;
cmd=(char *)0;
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(3,"%s Phase='%d' (done='%d')",OPEP,phase,done);
  switch (phase) {
    case 0	:	//preparing command and parametre
       if (vasprintf(&parmlst,fmt,args)<0) {
	(void) log_alert(0,"%s, Unable to assign memory for parmlst! (Bug?)",OPEP);
	phase=999;	/*trouble trouble	*/	
	}
      if (asprintf(&cmd,"%s/shell/%s %s",libexecpath,scriptname,parmlst)<0) {
	(void) log_alert(0,"%s, Unable to assign memory for cmd! (Bug?)",OPEP);
	phase=999;	/*trouble trouble	*/	
	}
      break;
    case 1	:	/*opening pip channel	*/
      if ((canal=popen(cmd,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s, Unable to pipe cmd <%s> (error=<%s>)",
			    OPEP,cmd,strerror(errno));
	phase=999;	/*trouble trouble	*/	
	}
      break;
    case 2	:	/*do we have feed back	*/
      if (canal!=(FILE *)0) {	/*always	*/
	char buffer[BSIZE];

	while (fgets(buffer,BSIZE,canal)!=(char *)0) {
	  (void) log_alert(2,"%s, pipe say: <%s>",appname,buffer);
	  (void) fprintf(stdout,"%s",buffer);
	  (void) fflush(stdout);
	  }
	}
      break;
    case 3	:	/*closing pip		*/
      if (pclose(canal)<0) {
	(void) log_alert(0,"%s, Unable to close pipe, cmd <%s> (error=<%s>)",
			    OPEP,cmd,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 4	:	//reporting everython OK
      done=true;
      break;
    default	:	/*SAFE Guard		*/
      parmlst=apl_freestr(parmlst);
      cmd=apl_freestr(cmd);
      proceed=false;
      break;
    }
  phase++;
  }
libexecpath=apl_freestr(libexecpath);
va_end(args);
return done;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to inject a command within	*/
/*	the container.				*/
/*						*/
/************************************************/
PUBLIC int cnt_injectcmd(const char *scriptname,char *contname,int contpid,char *params)

{
#define	OPEP	"unicnt.c:cnt_injectcmd,"
#define	FDS	3
#define	CMDSEQ	"%s/shell/%s %s %d %s"

int status;
char *cmd;
char *libexecpath;
int phase;
int proceed;

status=0;
cmd=(char *)0;
libexecpath=apl_appdir(d_libexec);
phase=0;
proceed=true;
if (asprintf(&cmd,CMDSEQ,libexecpath,scriptname,contname,contpid,params)<0) {
  (void) log_alert(0,"%s, Unable to generate command! (memory? bug?)",OPEP);
  proceed=false;
  }
while (proceed==true) {
  switch (phase) {
    case 0	:	//protecting already std devices?
      break;
    case 1	:	//executing the command
      status=system(cmd);
      break;
    case 2	:	//putting back devices
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
libexecpath=apl_freestr(libexecpath);
cmd=apl_freestr(cmd);
return status;
#undef OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to extract the container	*/
/*	architecture available in the 'arch' 	*/
/*	file.					*/
/*						*/
/************************************************/
PUBLIC char *cnt_getarch(char *contname)

{
static char *availarch[]={
	"i386","i686","x86_64",(char *)0
	};

char *arch;
FILE *fichier;
char *contpath;
char *filename;
int phase;
int proceed;
char buffer[200];

arch=(char *)0;
fichier=(FILE *)0;
filename=(char *)0;
contpath=getcontpath(contname);
filename=merge_str("%s/%s",contpath,"arch");
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	/*let open the file	*/
      if ((fichier=fopen(filename,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open file <%s> (error=<%s>)",
			   appname,filename,strerror(errno));
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 1	:	/*reading first line	*/
      if (apl_getstr(fichier,buffer,sizeof(buffer),'#')==(char *)0) {
	(void) log_alert(0,"%s Unable to read file <%s> (file empty?)",
			   appname,filename);
	phase=999;	/*trouble trouble	*/
	}
      (void) fclose(fichier);
      break;
    case 2	:	/*scanning line		*/
      (void) apl_cleanstring(buffer);
      if (strlen(buffer)>0) { 	/*always?	*/
	int i;

        for (i=0;availarch[i]!=(char *)0;i++) {
	  if (strcmp(availarch[i],buffer)==0) {
	    arch=strdup(availarch[i]);
	    proceed=false;	/*found arch	*/
	    break;
	    }
	  }
	}
      break;
    case 3	:	/*scanning line		*/
      (void) log_alert(0,"%s file <%s>, arch <%s> unknown, setting default",
			   appname,filename,buffer);
      break;
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
(void) apl_freestr(filename);
(void) apl_freestr(contpath);
if (arch==(char *)0) {
  arch=strdup(availarch[0]);
  }
return arch;
}
/*

*/
/************************************************/
/*						*/
/*	procedure to extract the container	*/
/*	distribution type available in the  	*/
/*	'dist' file.				*/
/*						*/
/************************************************/
PUBLIC char *cnt_getdist(char *contname)

{
char *dist;
FILE *fichier;
char *contpath;
char *filename;
int phase;
int proceed;
char buffer[200];

dist=(char *)0;
fichier=(FILE *)0;
filename=(char *)0;
contpath=getcontpath(contname);
filename=merge_str("%s/%s",contpath,"dist");
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	/*let open the file	*/
      if ((fichier=fopen(filename,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open file <%s> (error=<%s>)",
			   appname,filename,strerror(errno));
	phase=999;	/*trouble trouble	*/
	}
      break;
    case 1	:	/*reading first line	*/
      if (apl_getstr(fichier,buffer,sizeof(buffer),'#')==(char *)0) {
	(void) log_alert(0,"%s Unable to read file <%s> (file empty?)",
			   appname,filename);
	phase=999;	/*trouble trouble	*/
	}
      (void) fclose(fichier);
      break;
    case 2	:	/*scanning line		*/
      (void) apl_cleanstring(buffer);
      dist=strdup(buffer);
      break;
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
(void) apl_freestr(filename);
(void) apl_freestr(contpath);
if (dist==(char *)0) {
  dist=strdup("unknown");
  }
return dist;
}
/*

*/
/************************************************/
/*						*/
/*	procedure to assign a new STDOUT/STDERR	*/
/*	and close STDIN. Done ONLY if the	*/
/*	process is in background mode.		*/
/*						*/
/************************************************/
PUBLIC void cnt_setstdio(_Bool live,char *contname,char *outname)

{
#define	OPEP	"unicnt.c:cnt_setstdio,"

if (live==false) {	//are we in foreground (live) mode?
  int newstdout;
  int newstderr;
  int phase;
  int proceed;
  char *contpath;
  char *filename;

  newstdout=-1;
  newstderr=-1;
  phase=0;
  proceed=true;
  contpath=getcontpath(contname);
  filename=merge_str("%s/%s.stdout",contpath,outname);
  while (proceed==true) {
    switch (phase) {
      case 0	:	/*open the new stdout	*/
	(void) unlink(filename);
	if ((newstdout=open(filename,O_CREAT|O_RDWR|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP))<0) {
	  (void) log_alert(0,"%s, Unable to open <%s> (error=<%s>)",
			      OPEP,filename,strerror(errno));
	  phase=999;	/*trouble trouble	*/
	  }
	break;
      case 1	:	/*open the new stderr	*/
	(void) free(filename);
        filename=merge_str("%s/%s.stderr",contpath,outname);
	(void) unlink(filename);
	if ((newstderr=open(filename,O_CREAT|O_RDWR|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP))<0) {
	  (void) log_alert(0,"%s, Unable to open <%s> (error=<%s>)",
			      OPEP,filename,strerror(errno));
	  (void) close(newstdout);
	  phase=999;	/*trouble trouble	*/
	  }
	break;
      case 2	:	/*duplicating stdout	*/
	if (dup2(newstdout,1)<0) {
	  (void) log_alert(0,"%s, Unable to dup2 stdout (error=<%s>)",
			      OPEP,strerror(errno));
	  (void) close(newstderr);
	  (void) close(newstdout);
	  phase=999;
	  }
	break;
      case 3	:	/*duplicating stderr	*/
	(void) close(newstdout);
	if (dup2(newstderr,2)<0) {
	  (void) log_alert(0,"%s, Unable to dup2 stderr (error=<%s>)",
			      OPEP,strerror(errno));
	  (void) close(newstderr);
	  phase=999;
	  }
	break;
      case 4	:	/*setting new std	*/
	if ((stdout=fdopen(1,"w"))==(FILE *)0) {
	  (void) log_alert(0,"%s, Unable to fdopen stdout (error=<%s>)",
			      OPEP,strerror(errno));
	  }
	if ((stderr=fdopen(2,"w"))==(FILE *)0) {
	  (void) log_alert(0,"%s, Unable to fdopen stderr (error=<%s>)",
			      OPEP,strerror(errno));
	  }
	(void) fclose(stdin);
	break;
      case 5	:	/*write time stamp	*/
	(void) fprintf(stdout,"stdout start: %s\n",apl_ascsysdatetime(time((time_t *)0)));
	(void) fprintf(stderr,"stderr start: %s\n",apl_ascsysdatetime(time((time_t *)0)));
	(void) fflush(stdout);
	(void) fflush(stderr);
	break;
      default	:	/*SAFE Guard		*/
	proceed=false;
        break;
      }
    phase++;
    }
  (void) apl_freestr(filename);
  (void) apl_freestr(contpath);
  }
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to open the container console */
/*      FIFO. This link is used to report       */
/*      console screen to supervisor process    */
/*      console.                                */
/*						*/
/************************************************/
PUBLIC int cnt_open_cont_console(const char *contname)

{
#define OPEP    "unicnt.c:cnt_open_cont_console"

int cont_console;
char *contpath;
char *filename;

cont_console=-1;
contpath=getcontpath(contname);
filename=merge_str("%s/rootfs/dev/%s",contpath,"console");
if ((cont_console=open(filename,O_RDWR|O_ASYNC))<0) {
  (void) log_alert(0,"%s, Unable to open container console FIFP <%s> (error=<%s>)",
			      OPEP,filename,strerror(errno));
  }
(void) apl_freestr(filename);
(void) apl_freestr(contpath);
return cont_console;

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to wait for entry on the	*/
/*	container console and forward it	*/
/*	on the Master_container console file.	*/
/*	A Management expiration is done		*/
/*	according time.				*/
/*	Status is 0 and 2, if process shutdown	*/
/*	Status is 1, if process need reboot	*/
/*						*/
/************************************************/
PUBLIC int cnt_mstconsole(char *contname,pid_t cntpid,int cconsole)

{
#define	OPEP	"unicnt.c:cnt_cmstonsole,"
#define	RLX	5	//maximun number of wait time

int relaxtime;          //in seconds
int status;
int nbr;
int mconsole;
int epoll_fd;
struct epoll_event ev,events[1];
STATYP *contstat;;
char *contpath;
char *filename;
int phase;
int proceed;
char buffer[1000];

relaxtime=5;
status=0;
nbr=0;
mconsole=0;
contstat=sys_new_cont_status();
contpath=getcontpath(contname);
filename=merge_str("%s/%s",contpath,"console");
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d'",OPEP,phase);
  switch (phase) {
    case 0	:		//creating the pool event
      if ((epoll_fd=epoll_create1(0))<0) {
	(void) log_alert(0,"%s, container <%s> %s (error=<%s>)",
			    OPEP,contname,
                            "Unable to create event pool",strerror(errno));
	status=-1;
	phase=999;
        }
      break;
    case 1	:	        //preparing event to check container console
      ev.events=EPOLLIN; 
      ev.data.fd=cconsole;
      if (epoll_ctl(epoll_fd,EPOLL_CTL_ADD,cconsole,&ev)<0) {
	(void) log_alert(0,"%s, container <%s> %s (error=<%s>)",
			    OPEP,contname,
                            "Unable to set pool event type",strerror(errno));
	status=-1;
	phase=999;
        }
      break;
    case 2	:	        //opening the surpervisor console
      if ((mconsole=open(filename,O_CREAT|O_WRONLY|O_TRUNC,S_IRUSR|S_IWUSR))<0) {
	(void) log_alert(0,"%s, Unable to open mconsole <%s> (error=<%s>)",
			      OPEP,filename,strerror(errno));
	status=-1;
	phase=999;
	}
      break;
    case 3	:		/*waiting event		*/
      nbr=epoll_wait(epoll_fd,events,1,relaxtime*1000);
      switch (nbr) {
        case	-1    :
	  switch (errno) {
	    case EINTR:	/*received a signal, lets see...*/
	      if (apl_checksig()!=0)
		phase--;	//it a welcome message lets continue
	      break;
	    default   :
	      (void) log_alert(0,"%s, Container <%s> unexpected select "
			      	 "event (error=<%s>)",
				 OPEP,contname,strerror(errno));
	      break;
	    }
          break;
        case      0   :	/*timeout child lost ?	*/
	  nbr=waitpid(cntpid,&status,WNOHANG);
	  switch (nbr) {
	    case -1   :	/*exiting 		*/
	      (void) log_alert(0,"%s, Container <%s> Unexpected error on "
			      	 "waitpid (error=<%s>)",
				 OPEP,contname,strerror(errno));
	      status=-1;
	      break;
	    case 0    :	/*process still up!	*/
	      if (relaxtime<RLX)
		relaxtime++;
	      if (updateproc(contstat)==true)
	      //if (updateproc(infos,contname,cntpid)==true)
		phase--;
	      break;
	    default   :	/*process exited!	*/
	      (void) log_alert(2,"Container <%s> exited with waitpid "
			         "status <%d -> %s>",
				 contname,status,strsignal(status));
	      if (WIFSIGNALED(status)==false) {
	        (void) log_alert(2,"Container <%s> exit without signal",contname);
		status=0;
		}
	      else {
		int real_sig;

		real_sig=WTERMSIG(status);
	        (void) log_alert(1,"Container <%s> exited with signal <%d -> %s>",
				   contname,status,strsignal(real_sig));
		}
	      break;	/*going to close phase	*/
	    }
          break;
	default	      :	/*data ready in console*/
	  (void) memset(buffer,'\000',sizeof(buffer));
	  nbr=read(cconsole,buffer,sizeof(buffer)-1);
	  switch (nbr) {
            case -1     :       //Trouble;
	      (void) log_alert(0,"%s Container <%s> unable to read from console "
				 "(error=<%s>)",
				 OPEP,contname,strerror(errno));
              (void) sleep(1);  //Lets relax somewhat
	      break;
	    case 0	:	//no characters available
	      (void) log_alert(0,"%s Container <%s> read nothing from console "
				 "(error=<%s>)",
				 OPEP,contname,strerror(errno));
              if (WIFSIGNALED(status)==false)
	        (void) log_alert(0,"Container <%s> exit without signal",contname);
              (void) sleep(1);  //Lets relax somewhat
	      break;
            default     :       //characaters available
              buffer[nbr]='\000'; 
	      if (write(mconsole,buffer,nbr)<0) {
	        (void) log_alert(1,"Container <%s> unable to write <%s> to console "
				   "(error=<%s>)",
				   contname,buffer,strerror(errno));
	        }
              break;
	    relaxtime=1;
	    }		/*forwarded to mconsole	*/
	  phase--;	/*stay put on phase	*/
          break;
	}
      break;
    case 4	:	/*closing all console	*/
      (void) close(mconsole);
      break;
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
//infos=cfg_freeinfos(infos);
contstat=sys_free_cont_status(contstat);
(void) apl_freestr(filename);
(void) apl_freestr(contpath);
return status;

#undef	RLX
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to assign cgroup limits to	*/
/*	container.				*/
/*						*/
/************************************************/
PUBLIC _Bool cnt_setcgroup(const char *contname)

{
#define	OPEP	"unicnt.c:cnt_setcgroup"

static struct lim {
	const char *val;
	int cas;
	}lims[]={
		{PIDMAX,1},	//maximun number of PID within container
		{MEMMAX,2},	//maximun available memory to container
		{SWAPMAX,3},	//maximun swap memory to be used by container
		{NUMCPU,4},	//assigning CPU to the container
		{PWRCPU,5},	//assigning a container weight
		{MAXCPU,6},	//Limiting CPU usage in the hardway.
		{(const char *)0,0}
		};

_Bool isok;
struct lim *ptr;

isok=true;

ptr=lims;
while (ptr->val!=(char *)0) {
  const char *got;
  int cas;
  const char *value;

  got=ptr->val;
  cas=ptr->cas;
  ptr++;
  if ((value=getenv(got))==(char *)0) 
    continue;
  switch (cas) {
    case  1	:	//adjust pids.max
      isok=prc_setpidsmax(contname,value);
      break;
    case  2	:	//adjust maximun memory
      isok=prc_setmemmax(contname,value,false);
      break;
    case  3	:	//adjust maximun swap
      isok=prc_setmemmax(contname,value,true);
      break;
    case  4	:	//assign CPU to container
      isok=sys_setcpuset(contname,apl_getdouble(value));
      break;
    case  5	:	//set container weight
      isok=sys_set_cont_weight(contname,apl_getdouble(value));
      break;
    case  6	:	//set container weight
      isok=sys_set_cont_usage(contname,apl_getdouble(value));
      break;
    default	:
      (void) log_alert(0,"%s, Unexpected cgroup cas=<%s> (Bug?)",OPEP,got);
      isok=false;
      break;
    }
  if (isok==false)
    break;
  }
return isok;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to mount/umount the		*/
/*	container's infos directory to a tmpfs	*/
/*	area					*/
/*						*/
/************************************************/
PUBLIC _Bool cnt_create_infos()

{
#define	OPEP	"unicnt.c:cnt_create_infos"

_Bool isok;
char *homefs;
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=false;
homefs=(char *)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//what is the container home directory
      if ((homefs=getenv(HOMEFS))==(char *)0) {
        (void) log_alert(0,"%s, env value for <%s> missing (Config?)",
			    OPEP,HOMEFS);
	phase=999;
	}
      break;
    case 1	:	//defining mount target
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",homefs,INFODIR);
      if (mkdir(ppath,0750)<0) {
	switch (errno) {
	  case EEXIST	:	//mount point already exists
	    break;
	  default	:
	    (void) log_alert(0,"%s Unable to create directory <%s>, "
			       "(error=<%s> config?)",
			        OPEP,ppath,strerror(errno));
	    phase=999;		//no need to go further 
	    break;
	  }
	}
      break;
    case 2	:	//temp file is created
      isok=true;	//expect the best
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to make sure the supervisor is*/
/*	in a "No Internal Process" directory	*/
/*						*/
/************************************************/
PUBLIC _Bool cnt_launch(char *contname)

{
#define	OPEP	"unicnt.c:cnt_launch"

_Bool isok;
FILE *fichier;
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=false;
fichier=(FILE *)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//creating the cgroup supervisor
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",
                                        sys_get_sysfs(cgr_supervisors),
                                        contname);
      if (mkdir(ppath,0750)<0) {
	switch (errno)	{
	  case EEXIST	:	//directory already existing
	    break;		//nothing to do
	  default	:
            (void) log_alert(0,"%s Unable to create directory <%s> (error=<%s>)",
                                OPEP,ppath,strerror(errno));
	    phase=999;		//trouble trouble
	    break;		//nothing to do
	  }
	}
      break;
    case 1	:	//creating the cgroup supervisor
      (void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",
					  sys_get_sysfs(cgr_supervisors),
					  contname,
					  "cgroup.procs");
      if ((fichier=fopen(ppath,"w"))==(FILE *)0) {
        (void) log_alert(0,"%s Unable to open file <%s> (error=<%s>)",
                            OPEP,ppath,strerror(errno));
	phase=999;
	}
      break;
    case 2	:	//moving supervisor process to vzgot/supervisor/contname
      (void) fprintf(fichier,"%d\n",getpid());
      (void) fclose(fichier);
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to make sure to have both	*/
/*	container and supervisor cgroup clean	*/
/*	exit.					*/
/*	This must follow the namespace rule	*/
/*	"No Internal Process"			*/
/*						*/
/************************************************/
//procedure to do a complete container clean exit
PUBLIC _Bool cnt_exit(pid_t cpid,char *contname)

{
#define	OPEP	"unicnt.c:cnt_exit"

_Bool isok;
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=false;
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d'",OPEP,phase);
  switch (phase) {
    case 0	:	//making sure the container is out of action
      if ((cpid!=(pid_t)0)&&(kill(cpid,0)==0)) {    //clone up and running?
        (void) kill(cpid,SIGTERM);      //Give a chance
        (void) usleep(20000);           //waiting for kill
        if (kill(cpid,0)==0)
          (void) kill(cpid,SIGKILL);    //sure kill
        }
      break;
    case 1	:	//moving supervisor process to vzgot
      if (sys_move_to_cgroup(contname,cgr_top)==false) {
        (void) log_alert(0,"%s move <%s> superviseur to top cgroup *system?)",
			    OPEP,contname);
	phase=999;
        }
      (void) usleep(10000);	//small relax
      break;
    case 2	:	//removing container directory
#ifdef	OBSOLETE
//Seems not needed as containers is not existing anymore a that stage
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",
                                        sys_get_sysfs(cgr_containers),
                                        contname);
      if (rmdir(ppath)<0) 
        (void) log_alert(0,"%s Unable to remove dir <%s> (error=<%s>)",
                            OPEP,ppath,strerror(errno));
#endif
      break;
    case 3	:	//removing supervisor directory
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",
                                        sys_get_sysfs(cgr_supervisors),
                                        contname);
      if (rmdir(ppath)<0) {
        (void) log_alert(0,"%s Unable to remove dir <%s> (error=<%s>)",
                            OPEP,ppath,strerror(errno));
        }
      break;
    case 4	:	//everythin fine
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	OPEP
}
