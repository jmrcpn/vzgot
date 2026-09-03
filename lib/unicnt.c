// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*      Copyright:				*/
/*	 Jean-Marc Pigeon <jmp@safe.ca>	 2009	*/
/*						*/
/************************************************/
/*						*/
/*	UNICNT:					*/
/*	Implement all routine to handle       	*/
/*	container access.		        */
/*						*/
/************************************************/
#include        <sys/epoll.h>
#include        <sys/inotify.h>
#include        <sys/mount.h>
#include        <sys/mman.h>
#include        <sys/prctl.h>
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
#include	<poll.h>
#include	<sched.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<syslog.h>
#include	<time.h>
#include	<unistd.h>

#include	"lowapl.h"
#include	"lowtyp.h"
#include	"dbglog.h"
#include	"utlapl.h"
#include	"utlprc.h"
#include	"utlsys.h"
#include	"subcfg.h"
#include	"subprc.h"
#include	"unicnt.h"

#define PRG     "unicnt.c"

/*container filesystem				*/
#define	CLONPID	"first.pid"	/*cont process 1*/
#define	MTAB	"/etc/mtab"	/*mtab file	*/

#define	DDEV	"/dev"		/*system /dev	*/
#define	DEVFS	"devtmpfs"	/*/dev systype	*/

/*systemd suggest to have /proc/ and /sys to be	*/
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
^L
*/
/************************************************/
/*						*/
/*	Procedure to create a  directory, not   */
/*      report a fault, if directory is already */
/*      existing.                               */
/*						*/
/************************************************/
static int do_mkdir(const char *dirname,int mode)

{
#define OPEP    PRG":do_mkdir"

int status;

if ((status=mkdir(dirname,mode))<0) {
  switch (errno) {
    case EEXIST :       //Directory already existing
      if ((status=chmod(dirname,mode))<0) {
        (void) log_alert(0,"%s Can not change directory <%s> mode to '%o' "
                           "(error=<%s>)",OPEP,dirname,mode,strerror(errno));
        }
      break;
    default     :       //This a real error
      (void) log_alert(0,"%s Can not create directory <%s> (error=<%s>)",
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
/*	Procedure to skip a processor definition*/
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
		MK("addresses sizes",KEEP),
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
/*	procedure to feed the load average to	*/
/*	container speciel feed file		*/
/*	/etc/vzgot/loadavg			*/
/*						*/
/************************************************/
static _Bool setloadavg(STATYP *contstat,const char *target)

{
#define	OPEP	"unicnt.c:setloadavg,"

_Bool isok;
const char *contpath;
char ppath[PATH_MAX];
char buffer[64];
int fd;
int phase;
_Bool proceed;

isok=false;
contpath=sys_get_cont_path(contstat->contname);
(void) snprintf(ppath,sizeof(ppath),"%s/dev/%s",contpath,target);
(void) snprintf(buffer,sizeof(buffer),"%-62s\n",contstat->loadavg);
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d' delta_t='%lf'",OPEP,phase,delta_t);
  switch (phase) {
    case 0	:	//feeding
      if ((fd=open(ppath,O_RDWR))<0) {
	if (errno==ENOENT) {
          (void) log_alert(0,"%s lets create file <%s>",OPEP,ppath);
	  fd=open(ppath,O_RDWR|O_CREAT,0644);
	  }
        }
      if (fd<0) {
        (void) log_alert(0,"%s Unable to open file <%s> (error=<%s>)",
                            OPEP,ppath,strerror(errno));
	phase=999;	//potential trouble
	}
      break;
    case 1	:	//inserting loadavg within file (test purpose)
      if (pwrite(fd,buffer,strlen(buffer),0)<0) {
        (void) log_alert(0,"%s Unable to pwrite file <%s> (error=<%s>)",
                            OPEP,ppath,strerror(errno));
        (void) close(fd);
	phase=999;	//potential trouble
	}
      break;
    case 2	:	//changeing loadavg access mode
      if (fchmod(fd,0444)<0) {
        (void) log_alert(0,"%s Unable to change <%s> access mode (error=<%s> %s)",
			    OPEP,ppath,strerror(errno),"System?");
	phase=999;	//trouble trouble
        }
      (void) close(fd);
      break;
    case 3	:	//everything fine
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
const char *contpath;
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
contpath=sys_get_cont_path(contname);
(void) snprintf(fname,sizeof(fname),"%s/dev/%s",contpath,target);
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
    case 7	:	//opening the meminfo name
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
      break;
    case 9	:	//be sure about the file acess mode
      if (fchmod(fileno(fmem),0444)<0) {
        (void) log_alert(0,"%s Unable to change <%s> access mode (error=<%s> %s)",
			    OPEP,fname,strerror(errno),"System?");
	phase=999;	//trouble trouble
        }
      (void) fclose(fmem);
      break;
    case 10	:	//everything fine
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
/*	procedure to generate a special swaps 	*/
/*	file.					*/
/*						*/
/************************************************/
static _Bool setswapsinfo(const char *contname,const char *target)

{
#define	OPEP	"unicnt.c:setswapsinfo"

_Bool isok;
FILE *fichier;
const char *contpath;
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=false;
fichier=(FILE *)0;
contpath=sys_get_cont_path(contname);
(void) snprintf(ppath,sizeof(ppath),"%s/dev/%s",contpath,target);
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
      break;
    case 2	:	//changing access mode
      if (fchmod(fileno(fichier),0444)<0) {
        (void) log_alert(0,"%s Unable to change <%s> access mode (error=<%s> %s)",
			    OPEP,ppath,strerror(errno),"System?");
	phase=999;	//trouble trouble
        }
      (void) fclose(fichier);
      break;
    case 3	:	//everythin is fine
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
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
const char *contpath;
char ppath[PATH_MAX];
PHYCPU hostcpus;
PHYCPU contcpus;
int numcpu;
int phase;
_Bool proceed;

isok=false;
fin=(FILE *)0;
fout=(FILE *)0;
contpath=sys_get_cont_path(contname);
(void) snprintf(ppath,sizeof(ppath),"%s/dev/%s",contpath,target);
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
      (void) fclose(fin);
      break;
    case 5	:	//make sur the cpuinfo access mode
      if (fchmod(fileno(fout),0444)<0) {
        (void) log_alert(0,"%s Unable to change <%s> access mode (error=<%s> %s)",
			    OPEP,ppath,strerror(errno),"System?");
	phase=999;	//trouble trouble
        }
      (void) fclose(fout);
      break;
    case 6	:	//everything is fine
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
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
PUBLIC _Bool cnt_updateproc(STATYP *contstat)

{
#define	OPEP	PRG":updateproc"
#define	PACE	5.0		//number of second pace update

static TIMETIC last_time={0,0};

_Bool isok;
double delta_t;
const DEVTYP *specdevs;
TIMETIC cur_time;
int phase;
_Bool proceed;

isok=true;
delta_t=(double)0.0;
specdevs=apl_get_specdevs();
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
      for (int i=0;specdevs[i].str!=(const char *)0;i++) {
	switch (specdevs[i].devenum) {
	  case dev_loadavg	:	// computing loadavg
	    (void) setloadavg(contstat,specdevs[i].str);
	    break;
	  case dev_meminfo	:	// computing meminfo
	    (void) setmeminfo(contstat->contname,specdevs[i].str);
	    break;
	  case dev_swaps	:	// generating swaps information
	    (void) setswapsinfo(contstat->contname,specdevs[i].str);
	    break;
	  case dev_cpuinfo	:	// generating CPU information
	    (void) setcpuinfo(contstat->contname,specdevs[i].str);
	    break;
          case dev_lastpid      :       //nothing to do
          case dev_acpi         :
          case dev_bus          :
          case dev_interrupts   :
          case dev_ioports      :
          case dev_kcore        :
          case dev_mdstat       :
          case dev_modules      :
          case dev_partitions   :
          case dev_trigger      :
	    break;
	  default	        :	//unexpected case report!
            (void) log_alert(0,"%s Unexpected <%s> target! (Bug?)",
                                OPEP,specdevs[i].str);
	    break;
	  }
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
/*	Procedure to assign cgroup limits to	*/
/*	container.				*/
/*						*/
/************************************************/
static _Bool cgroup_limits(const char *contname)

{
#define	OPEP	PRG":cgroup_limits"

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
/*	Procedure to set the container working	*/
/*	"root" user at kernel level, used by	*/
/*	container with CLONE_NEWUSER mode.	*/
/*						*/
/************************************************/
PUBLIC _Bool cnt_mapcontids(const char *contname,pid_t clonepid,uid_t contuid,gid_t contgid)

{
#define OPEP    PRG":cnt_mapcontids"
#define MXMAP   sizeof(names)/sizeof(char *)

static char *names[]={
        "uid_map",
        "setgroups",
        "gid_map"
        };


_Bool done;
const char *contpath;
char rootfs[512];
char locnames[MXMAP][100];
char data[MXMAP][100];
int phase;
_Bool proceed;

done=false;
contpath=sys_get_cont_path(contname);
(void) snprintf(rootfs,sizeof(rootfs),"%s/%s",contpath,"rootfs");
(void) memset(locnames,'\000',sizeof(locnames));
(void) memset(data,'\000',sizeof(data));
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(1,"%s JMPDBG phase='%d', rootfs=<%s>",OPEP,phase,rootfs);
  switch (phase) {
    case 0        :     //preparing file name
      for (int i=0;i<MXMAP;i++) 
        (void) snprintf(locnames[i],sizeof(locnames[i]),
                         "/proc/%d/%s",clonepid,names[i]);
      break;
    case 1        :     //preparing data
      (void) snprintf(data[0],sizeof(data[0]),"0 %d %d",contuid,65536);
      (void) snprintf(data[1],sizeof(data[1]),"allow");
      (void) snprintf(data[2],sizeof(data[2]),"0 %d %d",contgid,65536);
      break;
    case 2        :     //storing data in process map
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
    case 3      :    //everyhing is right, lets report it
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
^L
*/
/************************************************/
/*						*/
/*	Procedure to prepare a supervisor       */
/*      dev directory to be shared with         */
/*      containers.                             */
/*						*/
/************************************************/
PUBLIC _Bool cnt_set_sup_devices(const char *contname)

{
#define OPEP    PRG":cnt_set_sup_devices"
#define CPN     "cp -a %s/. %s/"

_Bool isok;
const char *contpath;
const DEVTYP *specdevs;
int mode;
char sdev[512];
char devref[512];
char cmd[2048];
int phase;
_Bool proceed;

isok=false;
mode=0;
sdev[0]='\000';
devref[0]='\000';
cmd[0]='\000';
contpath=sys_get_cont_path(contname);
specdevs=apl_get_specdevs();
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //make sure the supervisor /dev file is existing
      (void) snprintf(sdev,sizeof(sdev),"%s/dev",contpath);
      if (do_mkdir(sdev,0755)<0) 
        goto TOOBAD;
      break;
    case 1      :       //mounting the supervisor /dev directory as tmpfs
      mode=0;
      if (mount("tmpfs",sdev,"tmpfs",mode,"size=4M,mode=0755")<0) {
        switch (errno) {
          case EBUSY    :       //dev dir alreay mounted (This is acceptable)
            break;
          default       :       //real error
            (void) log_alert(0,"%s, Unable to mount <%s> as tmpfs, (error=<%s>)",
                               OPEP,sdev,strerror(errno));
            goto TOOBAD;
            break;
          }
        }
      break;
    case 2      :       //Lets duplicate all allowed container nodes to dev
      (void) snprintf(devref,sizeof(devref),"%s/devref",contpath);
      (void) snprintf(cmd,sizeof(cmd),CPN,devref,sdev);
      if (system(cmd)!=0) {
        (void) log_alert(0,"%s, unable to execute commande <%s>",OPEP,cmd);
        (void) umount2(sdev,MNT_DETACH);
        goto TOOBAD;
        }
      break;
    case 3      :       //create all special devices
      for (int i=0;specdevs[i].str!=(const char *)0;i++) {
        int fd;

        (void) snprintf(sdev,sizeof(sdev),"%s/dev/%s",contpath,specdevs[i].str);
	if ((fd=open(sdev,O_RDWR|O_CREAT,0600))<0) {
          (void) log_alert(0,"%s, Unable to create file <%s>, (error=<%s>)",
                               OPEP,sdev,strerror(errno));
          }  
        (void) close(fd);
        }
      break;
    case 4      :       //Adding fd symlink
      (void) snprintf(sdev,sizeof(sdev),"%s/dev/fd",contpath);
      if (symlink("/proc/self/fd",sdev)<0) {
        switch (errno) {
          case EBUSY    :       //fd alreay existeing (This is acceptable)
            break;
          default       :       //real error
            (void) log_alert(0,"%s, Unable to symlink <%s> (error=<%s>)",
                               OPEP,sdev,strerror(errno));
            goto TOOBAD;
            break;
          }
        }
      break;
    case 5      :       //Everything is fine
      isok=true;
      break;
    TOOBAD      :
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
^L
*/
/************************************************/
/*						*/
/*	Procedure to free the supervisor        */
/*      dev directory.                          */
/*						*/
/************************************************/
PUBLIC _Bool cnt_unset_sup_devices(const char *contname)

{
#define OPEP    PRG":cnt_unset_sup_devices"

_Bool isok;
const char *contpath;
char sdev[1024];

isok=true;
contpath=sys_get_cont_path(contname);
(void) snprintf(sdev,sizeof(sdev),"%s/dev",contpath);
if (umount2(sdev,MNT_DETACH)<0) {
  (void) log_alert(0,"%s, unable to umount <%s> (error=<%s>",
                      OPEP,sdev,strerror(errno));
  isok=false;
  }
return isok;

#undef  OPEP
}
/*
^L
*/
/************************************************/
/*						*/
/*	Procedure to create all directory within*/
/*      container rootfs directory              */
/*      configuration                           */
/*						*/
/************************************************/
PUBLIC _Bool cnt_set_rootfs_dir(const char *contname)

{
static struct {
          const char *name;
          int mode;
          }needed[]={
            {"dev",0755},
            {"old_root",0755},
            {"proc",0555},
            {"run",01777},
            {"sys",0555},
            {"tmp",01777},
            {(const char *)0,0}
            };
_Bool isok;
const char *contpath;

isok=true;
contpath=sys_get_cont_path(contname);
for (int i=0;needed[i].name!=(const char *)0;i++) {
  char ppath[PATH_MAX];

  (void) snprintf(ppath, sizeof(ppath),"%s/rootfs/%s",contpath,needed[i].name);
  if (do_mkdir(ppath,needed[i].mode)<0) {
    isok=false;
    }
  }
return isok;
}
/*
^L
*/
/************************************************/
/*						*/
/*	Procedure to unset all monted directory */
/*						*/
/************************************************/
PUBLIC _Bool cnt_unset_rootfs_dir(const char *contname)

{
#define OPEP    PRG":cnt_unset_rootfs_dir"

static const char *dirs[]={"run","sys","proc",(char *)0};

_Bool isok;
const char *contpath;

isok=true;
contpath=sys_get_cont_path(contname);
for (int i=0;dirs[i]!=(const char *)0;i++) {
  char ppath[PATH_MAX];

  (void) snprintf(ppath, sizeof(ppath),"%s/rootfs/%s",contpath,dirs[i]);
  if (umount2(ppath,MNT_DETACH)<0) {
    switch (errno) {
      case EINVAL       :       //previously unmount (systemd?)
        break;
      default           :
        (void) log_alert(0,"%s, unable to umount <%s> (error=<%s>)",
                            OPEP,ppath,strerror(errno));
        isok=false;
        break;
      }
    if (isok==false)
      break;                    //not going further by purpose
    }
  }
return isok;

#undef  OPEP
}
/*
^L
*/
/************************************************/
/*						*/
/*	Procedure to prepare the cgroup         */
/*      configuration                           */
/*						*/
/************************************************/
PUBLIC _Bool cnt_set_cgroup(const char *contname)

{
#define SUBTREE "cgroup.subtree_control"
#define PRIVS   "+memory +pids +cpu +cpuset"

_Bool isok;
char sysfs[512];
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=false;
ppath[0]='\000';
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //make sure the the application directory is existing
      (void) sys_get_sysfs(sysfs,sizeof(sysfs),cgr_vzgot);
      if (do_mkdir(sysfs,0755)<0) 
        goto TOOBAD;
      break;
    case 1      :       //set the subtree control
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",sysfs,SUBTREE);
      if (sys_write_str(ppath,PRIVS)==false) 
        goto TOOBAD;
      break;
    case 2      :       //preparing supervisor cgroup
      (void) sys_get_sysfs(sysfs,sizeof(sysfs),cgr_supervisors);
      if (do_mkdir(sysfs,0755)<0)
        goto TOOBAD;
      break;
    case 3      :       //preparing containers cgroup
      (void) sys_get_sysfs(sysfs,sizeof(sysfs),cgr_containers);
      if (do_mkdir(sysfs,0755)<0) 
        goto TOOBAD;
      break;
    case 4      :       //adding controle capability to containers cgroups
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",sysfs,SUBTREE);
      if (sys_write_str(ppath,PRIVS)==false) 
        goto TOOBAD;
      break;
    case 5      :       //create cgroup path for container's supervisors
      (void) sys_get_sysfs(sysfs,sizeof(sysfs),cgr_supervisors);
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",sysfs,contname);
      if (do_mkdir(ppath,0755)<0)
        goto TOOBAD;
      break;
    case 6      :       //create cgroup paths for container itself
      (void) sys_get_sysfs(sysfs,sizeof(sysfs),cgr_containers);
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",sysfs,contname);
      if (do_mkdir(ppath,0755)<0)
        goto TOOBAD;
      break;
    case 7      :       //set all process limits
      if (cgroup_limits(contname)==false) 
        goto TOOBAD;
      break;
    case 8      :       //assign current process (supervisor) within it cgroup
      (void) sys_move_to_cgroup(contname,cgr_supervisors,getpid());
      isok=true;
      break;
    TOOBAD      :       //emergency exit
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef  PRIVS
#undef  SUBTREE
}
/*

*/
/************************************************/
/*						*/
/*	procedure to set a file with the clone	*/
/*	ip number.				*/
/*						*/
/************************************************/
PUBLIC _Bool cnt_set_cont_pid(const char *contname,pid_t cpid)

{
#define	OPEP	PRG":cnt_setclonepid,"

_Bool isok;
FILE *fichier;
char filename[1024];
const char *contpath;
int phase;
int proceed;

isok=false;
fichier=(FILE *)0;
contpath=sys_get_cont_path(contname);
(void) snprintf(filename,sizeof(filename),"%s/%s",contpath,CLONPID);
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
	(void) log_alert(0,"%s, Unable to write pidfile in container <%s> "
                           "(error=<%s>)",appname,contname,strerror(errno));
	phase=999;	/*trouble trouble	*/
	}
      (void) fclose(fichier);
      break;
    case 2	:	/*everything fine	*/
      isok=true;
      break;
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

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
PUBLIC pid_t cnt_get_cont_pid(const char *contname)

{
#define OPEP    PRG":cnt_get_cont_pid"

pid_t clonepid;
FILE *fichier;
char filename[1024];
char strloc[80];
const char *contpath;
int phase;
_Bool proceed;

clonepid=(pid_t)0;
contpath=sys_get_cont_path(contname);
(void) snprintf(filename,sizeof(filename),"%s/%s",contpath,CLONPID);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //Opening file
      if ((fichier=fopen(filename,"r"))==(FILE *)0) {
        switch (errno) {
          case ENOENT   :
	    (void) log_alert(4,"%s, pidfile <%s> not found (error=<%s>)",
                                OPEP,filename,strerror(errno));
            break;
          default       :
	    (void) log_alert(0,"%s, Unable to open pidfile <%s> (error=<%s>)",
                                OPEP,filename,strerror(errno));
            break;
          }
        goto TOOBAD;
        }
      break;
    case 1      :       //reading file
      if (fgets(strloc,sizeof(strloc)-1,fichier)==(char *)0) {
	(void) log_alert(0,"%s, Unable to read pidfile <%s> (error=<%s>)",
                           OPEP,filename,strerror(errno));
        phase=999;
        }
      (void) fclose(fichier);
      break;
    case 2      :       //scaning contents
      if (sscanf(strloc,"%d",&clonepid)!=1) {
	(void) log_alert(0,"%s, Unable to scan <%s> within pidfile <%s>",
                           OPEP,strloc,filename);
        goto TOOBAD;
        }
      break;
    TOOBAD      :
      clonepid=(pid_t)0;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return clonepid;

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to remove the file with the	*/
/*	clone pid.				*/
/*						*/
/************************************************/
PUBLIC int cnt_rm_cont_pid(const char *contname)

{
#define OPEP    PRG":cnt_rm_cnt_pid"

int done;
char filename[1024];
const char *contpath;

done=true;
contpath=sys_get_cont_path(contname);
(void) snprintf(filename,sizeof(filename),"%s/%s",contpath,CLONPID);
if (unlink(filename)<0) {
  (void) log_alert(0,"%s, Unable to remove pid file <%s> (error=<%s>)",
		     OPEP,filename,strerror(errno));
  done=false;
  }
return done;

#undef  OPEP
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
      (void) log_alert(1,"%s pipe command=<%s>",OPEP,cmd);
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
PUBLIC const char *cnt_getarch(char *contname)

{
static const char *availarch[]={
	"i386",
        "i686",
        "x86_64",
        (const char *)0
	};

const char *arch;
FILE *fichier;
const char *contpath;
char filename[1014];
int phase;
int proceed;
char buffer[200];

arch="UNK?";
fichier=(FILE *)0;
contpath=sys_get_cont_path(contname);
(void) snprintf(filename,sizeof(filename),"%s/%s",contpath,"arch");
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
	    arch=availarch[i];
            phase=999;  //arch found
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
PUBLIC const char *cnt_getdist(char *contname)

{
static char buffer[200];

FILE *fichier;
const char *contpath;
char filename[1024];
int phase;
int proceed;

(void) snprintf(buffer,sizeof(buffer),"%s","no dist");
fichier=(FILE *)0;
contpath=sys_get_cont_path(contname);
(void) snprintf(filename,sizeof(filename),"%s/%s",contpath,"dist");
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
      break;
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  }
return buffer;
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
  const char *contpath;
  char filename[1024];

  newstdout=-1;
  newstderr=-1;
  phase=0;
  proceed=true;
  contpath=sys_get_cont_path(contname);
  (void) snprintf(filename,sizeof(filename),"%s/%s.stdout",contpath,outname);
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
        (void) snprintf(filename,sizeof(filename),"%s/%s.stderr",contpath,outname);
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
	if (freopen((const char *)0,"w",stdout) == NULL) {
	  (void) log_alert(0,"%s, Unable to fdopen stdout (error=<%s>)",
			      OPEP,strerror(errno));
	  }
	if (freopen((const char *)0,"w",stderr) == NULL) {
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
  }
#undef	OPEP
}
#ifdef  ONSOLETE
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
#endif
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
char sysfs[512];
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=false;
fichier=(FILE *)0;
(void) sys_get_sysfs(sysfs,sizeof(sysfs),cgr_supervisors);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//creating the cgroup supervisor
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",sysfs,contname);
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
    case 1	:	//preparing pid transfer
      (void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",sysfs,contname,"cgroup.procs");
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
      if (sys_move_to_cgroup(contname,cgr_top,getpid())==false) {
        (void) log_alert(0,"%s move <%s> superviseur to top cgroup *system?)",
			    OPEP,contname);
	phase=999;
        }
      (void) usleep(10000);	//small relax
      break;
    case 2	:	//removing container directory
      break;
    case 3	:	//removing supervisor directory
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
/*

*/
/************************************************/
/*						*/
/*	Procedure to retrieve the container     */
/*      monitoring process. By design, within   */
/*      container, this process is PID 2 process*/
/*						*/
/************************************************/
PUBLIC  pid_t cnt_get_monitoring_pid(pid_t cont_pid)

{
#define OPEP    PRG":cnt_get_monitoring_pid"
#define PCHILD  "/proc/%d/task/%d/children"
#define MONPID  2

pid_t mpid;
char children_path[128];
FILE *cfile;
int phase;
_Bool proceed;

mpid=(pid_t)0;
(void) snprintf(children_path,sizeof(children_path),PCHILD,cont_pid,cont_pid);
cfile=(FILE *)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //Opening the process list
      if ((cfile=fopen(children_path,"r"))==(FILE *)0) {
        (void) log_alert(0,"%s Unable to open <%s> (error=<%s>",
			        OPEP,children_path,strerror(errno));
        goto TOOBAD;
        }
      break;
    case 1      :       //Scanning file
      while (fscanf(cfile,"%d",&mpid)==1) {
        char status_path[64];
        char line[256];
        FILE *mfile;

        (void)snprintf(status_path,sizeof(status_path),"/proc/%d/status",mpid);
        if ((mfile=fopen(status_path,"r"))==(FILE *)0) {
          (void) log_alert(0,"%s Unable to open status file <%s> (error=<%s>",
			      OPEP,status_path,strerror(errno));
          phase=999;
          break;        //no need to scan further
          }  
        while (fgets(line,sizeof(line),mfile)!=(char *)0) {
          if (strncmp(line,"NSpid:",6)==0)
            break;              //found it
          line[0]='\000';       //reset line
          }
        (void) fclose(mfile);
        if (line[0]!='\000') {  //lets check if it is the ri
          int parsed;
          int h_pid;
          int ns_pid;

          parsed=sscanf(line,"NSpid:\t%d\t%d",&h_pid,&ns_pid);
          if ((parsed==2)&&(ns_pid==MONPID)) {
            break;      //we just found the Monitor PID
            }
          }
        mpid=(pid_t)0;
        }
      (void) fclose(cfile);
      break;
    TOOBAD      :
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return mpid;

#undef  MONPID
#undef  PCHILD
#undef  OPEP
}

