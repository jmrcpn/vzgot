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
/*	Implement very sub level procedure to 	*/
/*	handle process sub function.		*/
/*						*/
/************************************************/
#include	<sys/prctl.h>
#include	<sys/resource.h>
#include	<sys/sysinfo.h>
#include	<sys/time.h>
#include	<sys/wait.h>
#include	<ctype.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<limits.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<time.h>
#include	<unistd.h>
#include	<utmp.h>

#include	"dbglog.h"
#include	"lowtyp.h"
#include	"utlapl.h"
#include	"utlprc.h"
#include	"utlsys.h"
#include	"subprc.h"

/*
^L
*/
/************************************************/
/*						*/
/*	Procedure to check if a child process	*/
/*	is still alive.				*/
/*						*/
/************************************************/
PUBLIC int prc_checkprocess(pid_t pid)

{
int status;

status=false;
switch(pid) {
  case (pid_t)0	:	/*0 means no process	*/
    status=false;
    break;
  case (pid_t)1	:	/*init process always OK*/
    status=true;
    break;
  default	:	/*standard process	*/
    if (kill(pid,SIGCHECK)==0)
      status=true;
    break;
  }
return status;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to allow exited child process	*/
/*	to leave the zombie stat.		*/
/*						*/
/************************************************/
PUBLIC void prc_nozombie()

{
while (waitpid(-1,(int *)0,WNOHANG)>0);
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to add a process number to	*/
/*	process list				*/
/*						*/
/************************************************/
PUBLIC PRCTYP *prc_addtoprc(PRCTYP *plst,pid_t pid)

{
register int taille;

taille=1;
if (plst==(PRCTYP *)0) {
  plst=(PRCTYP *)calloc(1,sizeof(PRCTYP));
  plst->pqueue=(pid_t *)calloc(taille,sizeof(pid_t));
  } 
plst->pqueue[plst->nbr]=pid;
plst->nbr++;
taille+=plst->nbr;
plst->pqueue=(pid_t *)realloc((void *)plst->pqueue,taille*sizeof(pid_t));
return plst;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to terminate all child process*/
/*	refered in a process list. if not	*/
/*	successful 'delay'  after SIGTERM try,	*/
/*	a SIGKILL signal is applied.		*/
/*						*/
/************************************************/
PUBLIC PRCTYP *prc_childkill(PRCTYP *prclst,int delay)

{
if (prclst!=(PRCTYP *)0) {
  pid_t *pid;
  int overkill;
  int i;

  overkill=false;
  pid=prclst->pqueue;
  (void) prc_nozombie();
  for (i=0;i<prclst->nbr;i++) {
    if (pid[i]<=1)	/*no way to kill admin process	*/
      continue;
    if (kill(pid[i],SIGTERM)==0)
      overkill=true;
    }
  while (overkill==true) {
    struct timespec timer;

    timer.tv_sec=1;
    timer.tv_nsec=0;
    overkill=false;
    while (nanosleep(&timer,&timer)!=0);
    (void) prc_nozombie();
    for (i=0;i<prclst->nbr;i++) {
      if (pid[i]<=1)	/*no way to kill admin process	*/
        continue;
      if (kill(pid[i],SIGCHECK)==0) 
	overkill=true;
      }
    delay--;
    if (delay==0)
      break;
    }
  if (overkill==true) {
    for (i=0;i<prclst->nbr;i++) {
      if (pid[i]<=1)	/*no way to kill admin process	*/
        continue;
      if (kill(pid[i],SIGCHECK)==0) {
				/*lets kill for sure		*/
        (void) kill(pid[i],SIGKILL);
        (void) fprintf(stderr,"prc_childkill, overkilled process'%05d'\n",
			       pid[i]);
        (void) fflush(stderr);
	}
      }
    }
  (void) prc_nozombie();
  (void) free(pid);
  (void) free(prclst);
  prclst=(PRCTYP *)0;
  }
return prclst;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to check exited process among	*/
/*	a process list, return only process	*/
/*	nomber still up and running.		*/
/*						*/
/************************************************/
PUBLIC PRCTYP *prc_childcheck(PRCTYP *prclst)

{
PRCTYP *newprc;

newprc=(PRCTYP *)0;
if (prclst!=(PRCTYP *)0) {
  if (prclst->nbr>0) {
    register int i;

    (void) prc_nozombie();
    for (i=0;i<prclst->nbr;i++) {
      if (prc_checkprocess(prclst->pqueue[i])==true) {
	newprc=prc_addtoprc(newprc,prclst->pqueue[i]);
	}
      }
    }
  if (prclst->pqueue!=(pid_t *)0)
    (void) free(prclst->pqueue);
  (void) free(prclst);
  }
return newprc;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to return the number of pids	*/
/*	the container is handling.		*/
/*	Note: PIDS_CURRENT usage from cgroup v2 */
/*						*/
/************************************************/
PUBLIC _Bool prc_cnt_pids_current(const char *contname,uint32_t *pids_current)

{
#define	OPEP	"utlprc.c:prc_cnt_pids_curent"

_Bool isok;
FILE *fp;
const char *sysfs;
char line[100];
char path[256];
int phase;
int proceed;

isok=false;
fp=(FILE *)0;
sysfs=sys_get_sysfs(cgr_containers);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//opening file
      (void) snprintf(path,sizeof(path),"/%s/%s/%s",sysfs,contname,"pids.current");
      if ((fp=fopen(path,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,path,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//extracting need value
      if (fgets(line,sizeof(line),fp)==(char *)0) {
	(void) log_alert(0,"%s Unable to read <%s> (error=<%s> system?)",
			    OPEP,path,strerror(errno));
	phase=999;	//trouble trouble
	}
      (void) fclose(fp);
      break;
    case 2	:	//extracting need value
      isok=true;	//lets say everythin will be fine
      if (line[0]!='\000') {	//always
	char *err;
	unsigned long result;

	err=(char *)0;
	result=strtoul(line,&err,10);
	if (errno == ERANGE || result > UINT32_MAX) {
	  (void) log_alert(0,"%s <%s> in not an uint32_t (error=<%s> bug?)",
			      OPEP,line,strerror(errno));
	  result=0;	//force the value
	  isok=false;
	  phase=999;	//trouble trouble
	  }
	*pids_current=(uint32_t)result;
	}
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
/*	Procedure to return the container CPU	*/
/*	current usage.				*/
/*	Note: CPU usage from cgroup v2 (usec)	*/
/*						*/
/************************************************/
PUBLIC _Bool prc_cnt_usage(const char *contname,u_vlong *usage)

{
#define	OPEP	"utlprc.c:prc_cnt_usage"
#define	FIELD	"usage_usec"

_Bool isok;
FILE *fp;
const char *sysfs;
char line[100];
char path[256];
int phase;
int proceed;

isok=false;
fp=(FILE *)0;
sysfs=sys_get_sysfs(cgr_containers);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//opening file
      (void) snprintf(path,sizeof(path),"/%s/%s/%s",sysfs,contname,"cpu.stat");
      if ((fp=fopen(path,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,path,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//extracting need value
      while (fgets(line,sizeof(line),fp)!=(char *)0) {
	if (strncmp(line,FIELD,strlen(FIELD))==0) {
	  char *ptr;

	  if ((ptr=strchr(line,' '))!=(char *)0) {
	    ptr++;
	    *usage=strtoull(ptr,(char **)0,10);
	    isok=true;
	    }
	  break;	//no need to scan further
	  }
	}
      (void) fclose(fp);
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	FIELD
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to return the container CPU	*/
/*	pressure total.				*/
/*	Note: CPU presure from cgroup v2 (usec)	*/
/*						*/
/************************************************/
PUBLIC _Bool prc_cnt_pressure(const char *contname,u_vlong *usage)

{
#define	OPEP	"utlprc.c:prc_cnt_presure"
#define	FIELD	"some"
//define FMT	"some %*[^=]=%*[^=]=%*[^=]=total=%llu"
#define FMT	"some avg10=%*f avg60=%*f avg300=%*f total=%llu"

_Bool isok;
FILE *fp;
const char *sysfs;
char line[100];
char path[256];
int phase;
int proceed;

isok=false;
fp=(FILE *)0;
sysfs=sys_get_sysfs(cgr_containers);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//opening file
      (void) snprintf(path,sizeof(path),"/%s/%s/%s",sysfs,contname,"cpu.pressure");
      if ((fp=fopen(path,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,path,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//extracting need value
      while (fgets(line,sizeof(line),fp)!=(char *)0) {
	(void) apl_cleanstring(line);
	if (strncmp(line,FIELD,strlen(FIELD))==0) {
	  int n;

	  isok=true;
	  if ((n=sscanf(line,FMT,usage))!=1) {
	    (void) log_alert(0,"%s scan of <%s> returned '%d' (unexpected format?)",
			        OPEP,line,n);
	    phase=999;	//trouble trouble
	    isok=false;
	    }
	  break;	//no need to scan further
	  }
	}
      (void) fclose(fp);
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	FMT
#undef	FIELD
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to set the maximun number of	*/
/*	PID within a container using cgroup.	*/
/*						*/
/************************************************/
PUBLIC _Bool prc_setpidsmax(const char *contname,const char *valeur)

{
#define	OPEP	"utlprc.c:setpidsmax"
#define	SYSPMAX	"/proc/sys/kernel/pid_max"
#define	MINMAX	100	//Minimal value

_Bool isok;
u_vlong sys_pid_max;
long maxpid;
const char *sysfs;
double ratio;
char ppath[200];
int phase;
_Bool proceed;

isok=false;
sys_pid_max=0;
maxpid=MINMAX;
sysfs=sys_get_sysfs(cgr_containers);
ratio=100.0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//getting the system pid_max
      if ((isok=sys_read_sys_long(SYSPMAX,&sys_pid_max))==false) 
	phase=999;	//No need to go further
      break;
    case 1	:	//getting the system pid_max
      ratio=apl_getdouble(valeur);
      maxpid=(long)((((double)sys_pid_max)*ratio)/100.0);
      if (maxpid<MINMAX)
	maxpid=MINMAX;
      break;
    case 2	:	//storing maxpix within the cgroup
      (void) snprintf(ppath,sizeof(ppath),"/%s/%s/%s",sysfs,contname,"pids.max");
      if ((isok=sys_write_sys_long(ppath,maxpid))==false) 
	phase=999;	//trouble trouble
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	MINMAX
#undef	SYSPMAX
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to set the maximun available	*/
/*	memory for the container		*/
/*						*/
/************************************************/
PUBLIC _Bool prc_setmemmax(const char *contname,const char *valeur,_Bool swap)

{
#define	OPEP	"utlprc.c:prc_setmemmax"
#define	ONEMEG	(1024*1024)
#define	MINMAX	32*ONEMEG		//32 Megs is the very minimal

_Bool isok;
struct sysinfo info;
double ratio;
const char *sysfs;
u_vlong totalmax;
u_vlong totalswap;
u_vlong memmax;
u_vlong swapmax;
char ppath[200];
int phase;
int proceed;

isok=false;
(void) memset(&info,'\000',sizeof(info));
ratio=0.0;
sysfs=sys_get_sysfs(cgr_containers);
totalmax=(u_vlong)0;
totalswap=(u_vlong)0;
memmax=(u_vlong)0;
swapmax=(u_vlong)0;
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d' swap='%d'",OPEP,phase,swap);
  switch (phase) {
    case 0	:	//getting kernel info
      if (sysinfo(&info)<0) {
	(void) log_alert(0,"%s Unable to get kernel sysinfo (error=<%s> system?)",
			    OPEP,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//computing HOST values
      memmax=((u_vlong)info.totalram*info.mem_unit);
      swapmax=((u_vlong)info.totalswap*info.mem_unit);
      break;
    case 2	:	//extracting need value
      ratio=apl_getdouble(valeur);
      totalmax=(u_vlong)((((double)memmax)*ratio)/100.0);
      totalswap=(u_vlong)((((double)swapmax)*ratio)/100.0);
      if (swap==false) {
        if (totalmax<MINMAX) {
	  (void) log_alert(0,"%s Requested Memory "
			     "%llu MBytes too low; "
			     "%llu MBytes set",
			      OPEP,
			      (u_vlong)(totalmax/ONEMEG),
			      (u_vlong)(MINMAX/ONEMEG));
	  totalmax=MINMAX;
	  }
	}
      break;
    case 3	:	//storing value in cgroup
      if (swap==false) {
        (void) snprintf(ppath,sizeof(ppath),"/%s/%s/%s",
					    sysfs,contname,"memory.swap.max");
        (void) sys_write_sys_long(ppath,0);	//preset swap to zero
        (void) snprintf(ppath,sizeof(ppath),"/%s/%s/%s",
					    sysfs,contname,"memory.max");
        if ((isok=sys_write_sys_long(ppath,totalmax))==false) 
	  phase=999;	//trouble trouble
	}
      else {
        (void) snprintf(ppath,sizeof(ppath),"/%s/%s/%s",
					    sysfs,contname,"memory.swap.max");
        if ((isok=sys_write_sys_long(ppath,totalswap))==false) 
	  phase=999;	//trouble trouble
	}
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	MINMAX
#undef	ONEMEG	
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to get the available memory	*/
/*	components dedicated to container	*/
/*						*/
/************************************************/
PUBLIC _Bool prc_getcgroupmem(const char *contname,const char *mname,u_vlong *cgmem)

{
#define	OPEP	"utlprc.c:prc_getcgroupmem"
#define	ONEMEG	(1024*1024)
#define	MINMAX	32*ONEMEG		//32 Megs is the very minimal

_Bool isok;
const char *sysfs;
char ppath[200];

isok=false;
sysfs=sys_get_sysfs(cgr_containers);
(void) snprintf(ppath,sizeof(ppath),"/%s/%s/%s",sysfs,contname,mname);
isok=sys_read_sys_long(ppath,cgmem);
return isok;

#undef	MINMAX
#undef	ONEMEG
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to retreive the number of	*/
/*	CPU assigned to the container.		*/
/*	Return the number of CPU on the HOST	*/
/*	if unable to retreive container value	*/
/*						*/
/************************************************/
PUBLIC uint16_t prc_getnbrcpu(const char *contname)

{
#define	OPEP	"utlprc.c:prc_getnbrcpu"
#define	FMT	"%s Unable to get container data <%s> (error=<%s> system?)"

uint16_t nbr;
FILE *fichier;
const char *sysfs;
char ppath[PATH_MAX];
char data[200];

int phase;
_Bool proceed;

nbr=sysconf(_SC_NPROCESSORS_CONF);
fichier=(FILE *)0;
sysfs=sys_get_sysfs(cgr_containers);
(void) snprintf(ppath,sizeof(ppath),"/%s/%s/%s",
				    sysfs,contname,"cpuset.cpus.effective");
(void) memset(data,'\000',sizeof(data));
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Getting the container data
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,FMT,OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//Reading file
      if (fgets(data,sizeof(data)-1,fichier)==(char *)0) {
	(void) log_alert(0,"%s Unable to get data from <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      (void) fclose(fichier);
      break;
    case 2	:	//counting cpu
      (void) apl_cleanstring(data);	//remove '\n' from line
      if (data[0]!='\000') {	//always
	char *ptr;

	nbr=0;		//strting cpu count
	ptr=data;
	while ((ptr!=(char *)0)&&(*ptr!='\000')) {
	  char *check;

	  check=ptr;
	  if ((ptr=strchr(check,','))!=(char *)0) {
	    *ptr='\000';	//cutting data
	    ptr++;
	    }
	  if (strspn(check,"0123456789-")>0) {
	    if (strchr(check,'-')!=(char *)0) {
	      int start;
	      int end;

	      if (sscanf(check,"%d-%d",&start,&end)==2) 
		nbr+=end-start;
	      }
	    nbr++;	//We got ONE cpu number
	    }
	  }
	}
      break;
    case 3	:	//making sure evything is right
      if (nbr==(uint16_t)0) {
	(void) log_alert(0,"%s No cpu number winthin <%s> (Amazing!)",OPEP,ppath);
	nbr=sysconf(_SC_NPROCESSORS_CONF);
	}
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return nbr;

#undef	FMT
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to retreive the host official	*/
/*	loadavg.				*/
/*						*/
/************************************************/
PUBLIC _Bool prc_host_loadavg(double *avg60,double *avg300,double *avg900)

{
#define	OPEP	"utlprc.c:prc_host_loadavg"
#define FMT     "%s Unable to open HOST loadavg <%s> (error=<%s> system?)"

_Bool isok;
FILE *fichier;
char ppath[PATH_MAX];
char data[200];
int phase;
_Bool proceed;

isok=false;
*avg60=0;
*avg300=0;
*avg900=0;
fichier=(FILE *)0;
(void) snprintf(ppath,sizeof(ppath),"/proc/loadavg");
(void) memset(data,'\000',sizeof(data));
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Getting the container data
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,FMT,OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//Reading file
      if (fgets(data,sizeof(data)-1,fichier)==(char *)0) {
	(void) log_alert(0,"%s Unable to get data from <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      (void) fclose(fichier);
      break;
    case 2	:	//scaning data
      isok=true;	//assum the best
      if ((sscanf(data,"%lf %lf %lf",avg60,avg300,avg900))!=3) {
	(void) log_alert(0,"%s Unable to scan data <%s> (Bug?)",OPEP,data);
	isok=false;	//best is not always true
	phase=999;
	}
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	FMT
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to retrieve the container	*/
/*	last assigned PID (to have a good	*/
/*	loadavg value).				*/
/*						*/
/************************************************/
PUBLIC _Bool prc_get_last_pid(const char *contname,uint32_t *last_pid)

{
#define	OPEP	"utlprc.c:prc_get_last_pid"
#define	FMT	"%s Unable to open <%s> (error=<%s> system?)"

_Bool isok;
FILE *fichier;
const char *sysfs;
uint32_t last_host_pid;
char *err;
char ppath[PATH_MAX];
char line[100];
int phase;
_Bool proceed;

isok=false;
*last_pid=(uint32_t)0;
fichier=(FILE *)0;
sysfs=sys_get_sysfs(cgr_containers);
last_host_pid=(uint32_t)0;
err=(char *)0;
(void) snprintf(ppath,sizeof(ppath),"/%s/%s/%s",sysfs,contname,"cgroup.procs");
(void) memset(line,'\000',sizeof(line));
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//reading file
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,FMT,OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//reading all line up to the end
      while (fgets(line,sizeof(line),fichier)!=(char *)0);
      if (line[0]=='\000') {
	(void) log_alert(0,"%s Unable to extract data from <%s> (Bug?)",
			    OPEP,ppath);
	phase=999;	//trouble trouble
	}
      (void) fclose(fichier);
      break;
    case 2	:	//we have the HOST last pid
      last_host_pid=strtol(line,&err,10);
      if ((*err!='\000')&&(!isspace((unsigned char)*err))) {
        (void) log_alert(0,"%s from <%s> unable to convert <%s> (system?)",
			    OPEP,ppath,line);
	phase=999;
	}
      break;
    case 3	:	//Lets have the container pid
      (void) memset(line,'\000',sizeof(line));
      (void) snprintf(ppath,sizeof(ppath),"/proc/%u/status",last_host_pid);
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,FMT,OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 4	:	//lets look for NSpid: line
      while (fgets(line,sizeof(line),fichier)!=(char *)0) {
	if (strncmp(line,"NSpid:",6)==0) {
	  isok=true;
	  //expected format is: "NSpid: <host_pid> <container_pid>"
	  break;	//we found the right line
	  
	  }
	}
      (void) fclose(fichier);
      if (isok==false) {
	(void) log_alert(0,"%s Unable to find a \"NSpid:\" line (kernel? bug?)",
			    OPEP);
	phase=999;
	}
      break;
    case 5	:	//so we got the right line
      if (line[0]!='\000') {	//always
	char *last_space;

	(void) apl_cleanstring(line);	//remove '\n'
	last_space=strrchr(line,'\t');	
	if (last_space==(char *)0)
	  last_space=strrchr(line,' ');
	if (last_space!=(char *)0) {
	  last_space++;
          *last_pid=strtol(last_space,&err,10);
          if ((*err!='\000')&&(!isspace((unsigned char)*err))) {
            (void) log_alert(0,"%s from <%s> unable to convert <%s> (system?)",
			       OPEP,ppath,line);
	    isok=false;
	    }
	  }
	}
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	FMT
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to parse the container utmp	*/
/*	file and extract the current number of	*/
/*	users.					*/
/*						*/
/************************************************/
PUBLIC _Bool prc_upd_users(pid_t contpid,u_int *nbrusr)

{
#define	OPEP	"subprc.c:prc_upd_users"

_Bool isok;
u_int count;
struct utmp *entry;
char utmp_path[PATH_MAX];
int phase;
_Bool proceed;

isok=false;
count=(u_int)0;
entry=(struct utmp *)0;
(void) snprintf(utmp_path,sizeof(utmp_path),"/proc/%d/root/var/run/utmp",contpid);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//check if utmp file is readable in container's structure
      if (access(utmp_path,R_OK)<0) {
        (void) log_alert(0,"%s No read access on file <%s> (error=<%s> %s)",
		              OPEP,utmp_path,strerror(errno),"system?");
	phase=999;
	}
      break;
    case 1	:	//counting users
      (void) utmpname(utmp_path);
      (void) setutent();
      while ((entry=getutent())!=(struct utmp *)0) {
	if ((entry->ut_type==USER_PROCESS)&&(entry->ut_user[0]))
	  count++;
	}
      (void) endutent();
      break;
    case 2	:	//Count completed successfully
      *nbrusr=count;
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
