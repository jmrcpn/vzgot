// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*	Implement low level procedure to 	*/
/*	handle CPU function.			*/
/*						*/
/************************************************/
#include	<arpa/inet.h>
#include	<sys/prctl.h>
#include	<sys/resource.h>
#include	<sys/stat.h>
#include	<sys/sysinfo.h>
#include	<sys/syscall.h>
#include	<sys/time.h>
#include	<sys/wait.h>
#include	<ctype.h>
#include	<cpuid.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<ifaddrs.h>
#include	<limits.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<syslog.h>
#include	<time.h>
#include	<unistd.h>
#include	<utmp.h>

#include	"dbglog.h"
#include	"lowtyp.h"
#include	"utlsys.h"

#define	SYSFS	"/sys/fs/cgroup/"
#define	SYSVZ	"/sys/fs/cgroup/"VZGOT"/"
#define	STARTFS	"starting"

#define WEIGHT	"cpu.weight"	//container or host, weight register
#define	CPUMAX	"cpu.max"	//cpu ceilling values
#define	CPUSTAT	"cpu.stat"	//system cpu usages
#define	CPUPRES	"cpu.pressure"	//system cpu pressure

//container selected processor list
static	PHYCPU cont_cpus_list;

/*

*/
/************************************************/
/*						*/
/*	Procedure to get to return measure unit	*/
/*	according value.			*/
/*						*/
/************************************************/
static double get_unit(double value,const char **unit)

{
static const char *unites[]={
			    " ",
			    "K",
			    "M",
			    "G",
			    "T",
			    };

int index;

index=0;
while ((value>=1024)&&(index<sizeof(unites))) {
  value/=1024;
  index++;
  }
*unit=unites[index];
return value;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to get the ip related	to an	*/
/*	interface name.				*/
/*						*/
/************************************************/
char *get_ip_num(char *intname)

{
#define	OPEP	"utlsys.c:get_ip_num"

char *ip_num;
struct ifaddrs *ifaddr;
struct ifaddrs *ifa;
int phase;
_Bool proceed;

ip_num=(char *)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//double check interface name
      if (intname==(char *)0) {
        (void) log_alert(0,"%s Network interface name missing (Bug!)",OPEP);
        phase=999;	//deep trouble trouble
	}
      break;
    case 1	:	//find ip reference
      if (getifaddrs(&ifaddr)<0) {
        (void) log_alert(0,"%s Unable to get addrs struct (error=<%s> Network?)",
			    OPEP,strerror(errno));
        phase=999;	//deep trouble trouble
        }
      break;
    case 2	:	//looking for interface
      for (ifa=ifaddr;ifa!=(struct ifaddrs *)0;ifa=ifa->ifa_next) {
	struct sockaddr_in *addr;
	char dest[INET_ADDRSTRLEN];

        if (ifa->ifa_addr && ifa->ifa_addr->sa_family!=AF_INET)
	  continue;
	if (strcmp(ifa->ifa_name,intname)!=0)
	  continue;
	addr=(struct sockaddr_in *)ifa->ifa_addr;	
	(void) inet_ntop(AF_INET,&(addr->sin_addr),dest,sizeof(dest));
	ip_num=strdup(dest);
	break;	//ipnum found
        }
      break;
    case 3	:	//free used memory
      (void) freeifaddrs(ifaddr);
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return ip_num;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to get the ip related	to an	*/
/*	interface name within a name space.	*/
/*						*/
/************************************************/
char *ns_get_ip_num(pid_t contpid,char *intname)

{
#define	OPEP	"utlsys.c:ns_get_ip_num"

char *ip_num;
char ppath[64];
int host_net_fd;
int cont_net_fd;
int phase;
_Bool proceed;

ip_num=(char *)0;
host_net_fd=0;
cont_net_fd=0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Keeping the Host namespace
      (void) snprintf(ppath,sizeof(ppath),"/proc/self/ns/net");
      if ((host_net_fd=open(ppath,O_RDONLY))<0) {
        (void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
        phase=999;	//deep trouble trouble
        }
      break;
    case 1	:	//Opening the Container namespace
      (void) snprintf(ppath,sizeof(ppath),"/proc/%d/ns/net",contpid);
      if ((cont_net_fd=open(ppath,O_RDONLY))<0) {
        (void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	(void) close(host_net_fd);
        phase=999;	//deep trouble trouble
	}
      break;
    case 2	:	//switch to Container namespace
      if (setns(cont_net_fd,CLONE_NEWNET)<0) {
        (void) log_alert(0,"%s Unable to reach <%s> namespace (error=<%s> %s)",
			    OPEP,ppath,strerror(errno),"system?");
	(void) close(cont_net_fd);
	(void) close(host_net_fd);
        phase=999;	//deep trouble trouble
	}
      break;
    case 3	:	//getting the ip_num within the container namespace
      if ((ip_num=get_ip_num(intname))==(char *)0) 
        (void) log_alert(0,"%s Warning! Unable to get ip_num from <%s> namespace)",
			    OPEP,ppath);
      break;
    case 4	:	//retreiving the host name space
      if (setns(host_net_fd,CLONE_NEWNET)<0) {
        (void) log_alert(0,"%s Unable to reach host namespace (error=<%s> %s)",
			    OPEP,strerror(errno),"system?");
	}
      (void) close(cont_net_fd);
      (void) close(host_net_fd);
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return ip_num;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to select CPU to be used	*/
/*	within container and update container	*/
/*	cgroup.					*/
/*	Used logic:				*/
/*	  Inverse-load weighted distribution.	*/
/*						*/
/************************************************/
static _Bool cpuselector(const char *contname,int maxcpu,double ratio)

{
#define	OPEP	"utlsys.c:cpuselector"
#define	PROSTAT	"/proc/stat"

_Bool isok;
FILE *fichier;
double sum_w;
double weights[maxcpu];
u_vlong work[maxcpu];
int cpu_ids[maxcpu];
const char *sysfs;
PHYCPU hostcpus;
PHYCPU todraw;
char buf[300];
int phase;
_Bool proceed;

isok=false;
fichier=(FILE *)0;
sum_w=0.0;
(void) memset(&weights,'\000',sizeof(weights));
(void) memset(&work,'\000',sizeof(work));
(void) memset(&cpu_ids,'\000',sizeof(cpu_ids));
(void) memset(&todraw,'\000',sizeof(todraw));
sysfs=sys_get_sysfs(cgr_containers);
//starting from scratch about CPU (global static)
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//get list cpu available on HOST
      if (sys_get_cpu_host_list(&hostcpus)==false) {
	(void) log_alert(0,"%s, unable to get HOST CPUs defintion (aborting!)",
			    OPEP);
	phase=999;
	}
      break;
    case 1	:	//preparing drawing
      if (ratio >100.0)
	ratio=100.0;
      hostcpus.selected=(long)((hostcpus.maxavail*ratio)/100.0);
      if (hostcpus.selected<=0)
	hostcpus.selected=1;
      todraw.maxcpu=hostcpus.maxcpu;
      todraw.maxavail=hostcpus.maxavail;
      todraw.selected=hostcpus.selected;
      break;
    case 2	:	//opening the CPU detail information
      if ((fichier=fopen(PROSTAT,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,PROSTAT,strerror(errno));
	phase=999;	//deep trouble trouble
	}
      break;
    case 3	:	//skip /proc/stat file fist line
      if (fgets(buf,sizeof(buf),fichier)==(char *)0) {
	(void) log_alert(0,"%s No first line in <%s>! (error=<%s> system?)",
			    OPEP,PROSTAT,strerror(errno));
        (void) fclose(fichier);
	phase=999;	//deep trouble trouble
	break;
	}
      break;
    case 4	:	//get available CPU weight
      for (int i=0;i<hostcpus.maxavail;i++) {
	u_vlong u;
	u_vlong n;
	u_vlong s;
	int cpu_id;

	if (CPU_ISSET(i,&(hostcpus.allocated))==0)
	  continue;	//cpu is NOT available
        if (fgets(buf,sizeof(buf),fichier)==(char *)0) {
	  (void) log_alert(0,"%s only '%d' lines within <%s> (Arch? system?)",
			      OPEP,i,PROSTAT);
	  phase=999;	//deep trouble trouble
	  break;	//No need to go further
	  }
	if (strncmp(buf,"cpu",3)!=0) {
	  (void) log_alert(0,"%s line <%s> from <%s> not an expected cpu%d line %s",
	      		      OPEP,buf,PROSTAT,i,"(arch? system?)");
	  phase=999;	//very deep trouble
	  break;	//No need to go further
	  }
	if (sscanf(buf,"cpu%d %llu %llu %llu",&cpu_id,&u,&n,&s)!=4) {
	  (void) log_alert(0,"%s Unable to scan <%s> line %d from <%s> %s",
	      		      OPEP,buf,i,PROSTAT,"(arch? system?)");
	  phase=999;	//very deep trouble
	  break;	//No need to go further
	  }
	if (cpu_id!=i) {
	  (void) log_alert(0,"%s cpu line from <%s> found cpu%d expected cpu%d %s",
	      		      OPEP,PROSTAT,cpu_id,i,"(arch? system?)");
	  phase=999;	//very deep trouble
	  break;	//No need to go further
	  }
	cpu_ids[i]=cpu_id;	
	work[i]=u+n+s;
	weights[i]=(1.0/(double)(work[i]+1));
	sum_w+=weights[i];
	}
      (void) fclose(fichier);
      break;
    case 5	:	//selecting a CPU set
      for (int i=0;i<todraw.selected;i++) {
        double load;
        double aleas;
        int start_cpu;

        load=0.0;
        aleas=((double)rand()/RAND_MAX)*sum_w;
	// Select random CPU as the first one
        start_cpu=rand()%todraw.maxavail;

        for (int j=0;j<todraw.maxavail;j++) {
	  int cpu;

          cpu=(start_cpu+j)%todraw.maxavail;
          if (weights[cpu]<0.0)
            continue;
          load+=weights[cpu];
          if (aleas<load) {
            (void) CPU_SET(cpu,&(todraw.allocated));
            sum_w-=weights[cpu];
            weights[cpu]=-1.0;
            break;
          }
        }
      }
      // we got the result!
      cont_cpus_list=todraw;
      break;
    case 6	:	//access the container cpuset file (from HOST)
      (void) snprintf(buf,sizeof(buf),"%s/%s/%s",sysfs,contname,"cpuset.cpus");
      if ((fichier=fopen(buf,"w"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> cgroup?)",
			    OPEP,buf,strerror(errno));
	phase=999;	//deep trouble trouble
	}
      break;
    case 7	:	//storing selected CPU to cgroup
      {
      char report[2048];

      (void) memset(report,'\000',sizeof(report));
      (void) memset(buf,'\000',sizeof(buf));
      for (unsigned int i=0;i<todraw.maxcpu;i++) {
	char ajout[1000];

	if (CPU_ISSET(i,&(todraw.allocated))==0)
	  continue;
	(void) fprintf(fichier,"%s%u",buf,i);
	(void) snprintf(ajout,sizeof(ajout),"%s%u",buf,i);
	(void) strcat(report,ajout);
	buf[0]=',';
	}
      (void) fprintf(fichier,"\n");
      (void) log_alert(0,"%s container <%s>, selected CPU=<%s>",
			  OPEP,contname,report);
      (void) fclose(fichier);
      }
      break;
    case 8	:	//storing todraw with container cache 
      cont_cpus_list=todraw;
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	PROSTAT
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to get Processors information	*/
/*						*/
/*	Note: the code segment is a direct	*/
/*	Gemini.google.com contribution.		*/
/*						*/
/************************************************/
static const char *get_cpu_model_name()


{
// 3 appels x 4 registres = 12 entiers (48 octets)
//
static unsigned int buffer[12];

(void) memset(buffer,'\000',sizeof(buffer));
// Le nom du modèle est divisé en 3 blocs consécutifs.
// On interroge les fonctions étendues 0x80000002, 0x80000003 et 0x80000004.
// Bloc 1
__cpuid(0x80000002,buffer[0],buffer[1],buffer[2],buffer[3]);

// Bloc 2
__cpuid(0x80000003,buffer[4],buffer[5],buffer[6],buffer[7]);

// Bloc 3
__cpuid(0x80000004,buffer[8],buffer[9],buffer[10],buffer[11]);

*(((char *)buffer)+sizeof(buffer)-1)='\000';
return (const char *)buffer;
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
static _Bool update_users(pid_t contpid,u_int *nbrusr)

{
#define	OPEP	"utlsys.c:upd_users"

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
/*

*/
/************************************************/
/*						*/
/*	Procedure to return the number of pids	*/
/*	the container is handling.		*/
/*	Note: PIDS_CURRENT usage from cgroup v2 */
/*						*/
/************************************************/
static _Bool cnt_pids_current(STATYP *status)

{
#define	OPEP	"utlsys.c:cnt_pids_current"
#define	CPIDS	"pids.current"

_Bool isok;
FILE *fichier;
const char *sysfs;
char ppath[PATH_MAX];
char line[100];
int phase;
int proceed;

isok=false;
fichier=(FILE *)0;
sysfs=sys_get_sysfs(cgr_containers);
(void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",sysfs,status->contname,CPIDS);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//opening file
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//extracting need value
      if (fgets(line,sizeof(line),fichier)==(char *)0) {
	(void) log_alert(0,"%s Unable to read <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      (void) fclose(fichier);
      break;
    case 2	:	//extracting need value
      if (sscanf(line,"%u\n",&(status->pids_current))!=1) {
	(void) log_alert(0,"%s unable to parse line  <%s> from file <%s>",
			    OPEP,line,ppath);
	phase=999;
	}
      break;
    case 3	:	//everything OK
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	CPIDS
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to collect the oom information*/
/*						*/
/************************************************/
static _Bool get_memory_stress(STATYP *status,CPUENU unit)

{
#define	OPEP	"utlsys.c:get_memory_stres"
#define	FEVENTS	"memory.events"
#define	FPRESS	"memory.pressure"
#define	OOMKILL "oom_kill "
#define	SOME	"some "

_Bool isok;
const char *sysfs;
u_int nbrline;
FILE *fichier;
char line[100];
char epath[PATH_MAX];
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=true;
sysfs=sys_get_sysfs(cgr_containers);
fichier=(FILE *)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Csetting proper file path
      switch (unit) {
	case cpu_host	:
	  (void) snprintf(epath,sizeof(epath),"%s/%s",sysfs,FEVENTS);
	  (void) snprintf(ppath,sizeof(ppath),"%s/%s",sysfs,FPRESS);
	  break;
	case cpu_cont	:
	  (void) snprintf(epath,sizeof(epath),"%s/%s/%s",
					       sysfs,status->contname,FEVENTS);
	  (void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",
					       sysfs,status->contname,FPRESS);
	  break;
	default		:
	  (void) log_alert(0,"%s Unexpected unit value='%d' (Bug!)",
			    OPEP,unit);
	  phase=999;	//code is very wrong somewhere!
	  break;
	}
      break;
    case 1	:	//getting oom_kill value
      if ((fichier=fopen(epath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,epath,strerror(errno));
	phase=999;	//trouble
	}
      break;
    case 2	:	//getting the value
      nbrline=100;
      while (fgets(line,sizeof(line),fichier)!=(char *)0) {
        if (sscanf(line,OOMKILL "%llu\n",&(status->oom[unit].oom_kill))==1) 
	  break;	//line found, breaking read
	nbrline--;
	if (nbrline==0) {
	  (void) log_alert(0,"%s Unable to get data from <%s> (system?)",
			      OPEP,epath);
	  phase=999;	//trouble trouble
	  break;	//breaking the loop, not searching forever
	  }
	}
      (void) fclose(fichier);
      break;
    case 3	:	//getting memory pressure
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble
	}
      break;
    case 4	:	//getting the value
      nbrline=100;
      while (fgets(line,sizeof(line),fichier)!=(char *)0) {
	if (sscanf(line,SOME "avg10=%lf",&(status->oom[unit].avg10))==1)
	  break;	//line found, breaking read
	nbrline--;
	if (nbrline==0) {
	  (void) log_alert(0,"%s Unable to get data from <%s> (system?)",
			      OPEP,ppath);
	  phase=999;	//trouble trouble
	  break;	//breaking the loop, not searching forever
	  }
	}
      (void) fclose(fichier);
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	SOME
#undef	FPRESS
#undef	FEVENTS
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to collect the memory		*/
/*	configuration and compute ratio		*/
/*	last assigned PID			*/
/*						*/
/************************************************/
static _Bool get_memory_info(STATYP *status,CPUENU unit,MEMENU mem)

{
#define	OPEP	"utlsys.c:get_memory_info"

static char *fname[][2]={
		   {"memory.max","memory.current",},
		   {"memory.swap.max","memory.swap.current"}
		   };

_Bool isok;
const char *sysfs;
FILE *fichier;
char line[100];
u_vlong max;
u_vlong used;
MEMINF *mems;
struct sysinfo info;
char mpath[PATH_MAX];
char upath[PATH_MAX];
u_vlong total[2];
int phase;
_Bool proceed;

isok=false;
sysfs=sys_get_sysfs(cgr_containers);
fichier=(FILE *)0;
mems=(MEMINF *)0;
max=(u_vlong)0;
used=(u_vlong)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//get sysinfo values
      if (sysinfo(&info)<0) {
	(void) log_alert(0,"%s Unable to get sysinfo! (error=<%s> system?)",
			    OPEP,strerror(errno));
	phase=999;
	}
      break;
    case 1	:	//setting the right path
      total[mem_mem]=((u_vlong)info.totalram)*info.mem_unit;
      total[mem_swap]=((u_vlong)info.totalswap)*info.mem_unit;
      switch (unit) {
	case cpu_host	:
	  (void) snprintf(mpath,sizeof(mpath),"%s/%s",sysfs,fname[mem][0]);
	  (void) snprintf(upath,sizeof(upath),"%s/%s",sysfs,fname[mem][1]);
	  break;
	case cpu_cont	:
	  (void) snprintf(mpath,sizeof(mpath),"%s/%s/%s",
					      sysfs,status->contname,fname[mem][0]);
	  (void) snprintf(upath,sizeof(upath),"%s/%s/%s",
					      sysfs,status->contname,fname[mem][1]);
	  break;
	default		:
	  (void) log_alert(0,"%s Unexpected unit value='%d' (Bug!)",
			    OPEP,unit);
	  phase=999;	//code is very wrong somewhere!
	  break;
	}
      break;
    case 2	:	//getting the max value
      if ((fichier=fopen(mpath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,mpath,strerror(errno));
	phase=999;	//trouble
	}
      break;
    case 3	:	//getting the value
      if (fgets(line,sizeof(line),fichier)==(char *)0) {
	(void) log_alert(0,"%s Unable to get data from <%s> (error=<%s> system?)",
			    OPEP,mpath,strerror(errno));
	phase=999;	//trouble trouble
	}
      (void) fclose(fichier);
      break;
    case 4	:	//getting the right value
      if (strcmp(line,"max\n")==0) 
	(void) snprintf(line,sizeof(line),"%llu\n",total[mem]);
      if (sscanf(line,"%llu\n",&max)!=1) {
	(void) log_alert(0,"%s Unable to scan max line <%s> (Bug?)",OPEP,line);
	phase=999;	//trouble trouble
	}
      break;
    case 5	:	//getting the max value
      if ((fichier=fopen(upath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,upath,strerror(errno));
	phase=999;	//trouble
	}
      break;
    case 6	:	//getting the value
      if (fgets(line,sizeof(line),fichier)==(char *)0) {
	(void) log_alert(0,"%s Unable to get data from <%s> (error=<%s> system?)",
			    OPEP,upath,strerror(errno));
	phase=999;	//trouble trouble
	}
      (void) fclose(fichier);
      break;
    case 7	:	//getting the right value
      if (sscanf(line,"%llu\n",&used)!=1) {
	(void) log_alert(0,"%s Unable to scan used line <%s> (Bug?)",OPEP,line);
	phase=999;	//trouble trouble
	}
      break;
    case 8	:	//setting values
      mems=&(status->mems[unit][mem]);
      mems->max=max;
      mems->used=used;
      mems->quotient=1;
      (void) memset(mems->unit,'\000',sizeof(mems->unit));
      if (max>0) {	//always
	static const char *unites[]={"","K","M","G"};

	int index;
	u_vlong quotient;

	index=0;
	quotient=1;
	while ((max>=1024)&&(index<3)) {
	  quotient*=1024;
	  max/=1024;
	  index++;
	  }
	mems->quotient=quotient;	
	(void) snprintf(mems->unit,sizeof(mems->unit),"%s",unites[index]);
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
/*	Procedure to collect on (received or	*/
/*	transmitted banwidth info.		*/
/*						*/
/************************************************/
static _Bool collect_count(BANTYP *bandwidth,char *path,double delta_t)

{
#define	OPEP	"utlsys.c:collect_count"

_Bool isok;
FILE *fichier;
u_vlong counter;
char line[200];
int phase;
_Bool proceed;

isok=false;
fichier=(FILE *)0;
counter=(u_vlong )0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//opening file
      if ((fichier=fopen(path,"r"))==(FILE *)0) {
        (void) log_alert(0,"%s, Unable to open <%s> (error=<%s>)",
                            OPEP,path,strerror(errno));
        phase=999;      //Trouble Trouble
        }
      break;
   case 1      :       //reading contents
      if (fgets(line,sizeof(line)-1,fichier)==(char *)0) {
        (void) log_alert(0,"%s, Unable to read <%s> (error=<%s>)",
                            OPEP,path,strerror(errno));
        phase=999;      //Trouble Trouble
        }
      (void) fclose(fichier);
      break;
    case 2	:	//parsing collected line
      if (sscanf(line,"%llu\n",&counter)!=1) {
        (void) log_alert(0,"%s, Unable to scan line <%s> from <%s> (Bug?)",
                            OPEP,line,path);
        phase=999;      //Trouble Trouble
        }
      break;
    case 3	:	//first measure
      isok=true;	//this ok
      if (bandwidth->cumulated==0) {
	bandwidth->cumulated=counter;
	phase=999;
	}
      break;
    case 4	:	//updating cummulated
      bandwidth->speed=(double)(counter-bandwidth->cumulated)/delta_t;
      bandwidth->cumulated=counter;
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
/*	Procedure to collect all banwidth info	*/
/*						*/
/************************************************/
static _Bool get_bandwidth_info(NETINF *network,double delta_t)

{
#define	OPEP	"utlsys.c:et_bandwidth_info"
#define STAT	"/sys/class/net/%s/statistics/%s"

static const char *band[]={"rx_bytes","tx_bytes"};


_Bool isok;
char ppath[PATH_MAX];

isok=true;
for (int i=0;i<2;i++) {
  (void) snprintf(ppath,sizeof(ppath),STAT,network->intname,band[i]);
  isok&=collect_count(&(network->traffic[i]),ppath,delta_t);
  }
return isok;

#undef	STATS
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to collect the container	*/
/*	last assigned PID			*/
/*						*/
/************************************************/
static _Bool get_last_pid(STATYP *status)

{
#define	OPEP	"utlsys.c:get_last_pid"
#define	MXCGR	100000		// Maximun number of alive PID
#define	NSPID	"NSpid:"

_Bool isok;
FILE *fichier;
const char *sysfs;
char ppath[PATH_MAX];
char line[64];
u_int nbrline;
pid_t last_host_pid;
char *ptr;
int phase;
_Bool proceed;

isok=false;
fichier=(FILE *)0;
sysfs=sys_get_sysfs(cgr_containers);
(void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",sysfs,status->contname,"cgroup.procs");
line[0]='\000';
nbrline=MXCGR;
ptr=(char *)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	// Is status avaliable
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble
	}
      break;
    case 1	:	// Reading all line
      while (fgets(line,sizeof(line),fichier)!=(char *)0) {
	nbrline--;
	if (nbrline==0) {
	  (void) log_alert(0,"%s Too many line within file <%s> (maxline='%d')",
			    OPEP,ppath,MXCGR);
	  phase=999;
	  break;
	  }
	}
      (void) fclose(fichier);
      break;
    case 2	:	// Is last line good?
      if (line[0]=='\000') {
	(void) log_alert(0,"%s file <%s> No data available!i (system?))",
			    OPEP,ppath);
	phase=999;
	}
      break;
    case 3	:	// Look for the last Host PID
      if (sscanf(line,"%d\n",&(last_host_pid))!=1) {
	(void) log_alert(0,"%s file <%s> line<%s> Not parsable! (bug?))",
			    OPEP,ppath,line);
	phase=999;
	}
      break;
    case 4	:	// Lets have the container pid
      (void) snprintf(ppath,sizeof(ppath),"/proc/%u/status",last_host_pid);
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble
	}
      break;
    case 5	:	// Looking for NSpid:
      nbrline=MXCGR;
      while (fgets(line,sizeof(line),fichier)!=(char *)0) {
	if (strncmp(line,NSPID,sizeof(NSPID)-1)==0)
	  break;
	nbrline--;
	if (nbrline==0) {
	  (void) log_alert(0,"%s Too many line within file <%s> (maxline='%d')",
			    OPEP,ppath,MXCGR);
	  phase=999;
	  break;
	  }
	}
      if (ferror(fichier)!=0) {
	(void) log_alert(0,"%s Unexpected reading error with file <%s>",
			    OPEP,ppath);
	phase=999;
	}
      (void) fclose(fichier);
      break;
    case 6	:	// Looking for last pid number
      if ((ptr=strrchr(line,'\t'))==(char *)0) {
	(void) log_alert(0,"%s unable to parse line  <%s> from file <%s>",
			    OPEP,line,ppath);
	phase=999;
	break;
	}
    case 7	:	//extracting pid
      isok=true;
      if (sscanf(ptr," %d",&(status->last_pid))!=1) {
	(void) log_alert(0,"%s line  <%s> from file <%s> no pid found!",
			    OPEP,line,ppath);
	isok=false;
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

#undef	NSPID
#undef	MXCGR
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to list namespace owner shipt	*/
/*	for a process.				*/
/*						*/
/************************************************/
PUBLIC void sys_show_namespace()

{
const char *ns_types[]={
		"mnt",
		"pid",
		"net",
		"ipc",
		"uts",
		"user",
		"cgroup",
		"time"
		};

char path[256];
struct stat st;

(void) log_alert(0,"Namespace Inodes used by process '%d'",getpid());
for (int i = 0; i < 8; i++) {
  snprintf(path, sizeof(path), "/proc/self/ns/%s", ns_types[i]);
  if (stat(path, &st) == 0) {
    (void) log_alert(0,"%s: %lu\n",ns_types[i],(unsigned long)st.st_ino);
    }
  }
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to clean and detach all	*/
/*	descriptor when a process is forked	*/
/*						*/
/************************************************/
PUBLIC _Bool sys_detach_from_supervisor(char *contname)

{
#define	OPEP	"utlsys.c:sys_detach_from_supervisor"
#define DEVCONS "/dev/console"

_Bool isok;
char report[300];
char signon[80];
int max_fd;
int devcons;
int phase;

_Bool proceed;

isok=false;
(void) memset(report,'\000',sizeof(report));
max_fd=sysconf(_SC_OPEN_MAX);
(void) snprintf(signon,sizeof(signon),"\nContainer <%s> console Ready\n",contname);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Open the /dev/console chanel
      if ((devcons=open(DEVCONS,O_RDWR))<0) {
        (void) snprintf(report,sizeof(report),
                        "%s, Unable to open <%s> (error=<%s>)",
                         OPEP,DEVCONS,strerror(errno));
	phase=999;	//Trouble
	}
      break;
    case 1	:	//redirecting stdin
      if (dup2(devcons,STDIN_FILENO)<0) {
        (void) snprintf(report,sizeof(report),
                        "%s, Unable to dup <%s> (error=<%s>)",
                         OPEP,"STDIN",strerror(errno));
	phase=999;	//Trouble
	}
      break;
    case 2	:	//redirecting stdout
      if (dup2(devcons,STDOUT_FILENO)<0) {
        (void) snprintf(report,sizeof(report),
                        "%s, Unable to dup <%s> (error=<%s>)",
                         OPEP,"STDOUT",strerror(errno));
	phase=999;	//Trouble
	}
      break;
    case 3	:	//redirecting stderr
      if (dup2(devcons,STDERR_FILENO)<0) {
        (void) snprintf(report,sizeof(report),
                        "%s, Unable to dup <%s> (error=<%s>)",
                         OPEP,"STDERR",strerror(errno));
	phase=999;	//Trouble
	}
      (void) close(devcons);
      break;
    case 4	:	//writing to devices
      (void) snprintf(signon,sizeof(signon),"STDOUT: Container=<%s>\n",contname);
      if (write(STDOUT_FILENO,signon,strlen(signon))<0) 
        (void) snprintf(report,sizeof(report),
                        "%s, Unable to write signon  <%s> to <%s> (error=<%s>)",
                         OPEP,signon,"STDOUT",strerror(errno));
      (void) syncfs(STDOUT_FILENO);
      break;
    case 5	:	//closing devcons
      for (int i = 3; i < max_fd; i++) 
        close(i);	//closing all other pending connection
      isok=true;
      break;
    default	:	//SAFE Guard
      if (strlen(report)>0) {
        FILE *atlast;

        if ((atlast=fopen("/Boot.report","w"))!=(FILE *)0) {
          (void) fprintf(atlast,"%s...\n",report);
          (void) fclose(atlast);
          }
        }
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
/*	Procedure to return the cgroup sysfs	*/
/*	value					*/
/*						*/
/************************************************/
PUBLIC const char *sys_get_sysfs(CGRENU ext)

{
#define	OPEP	"utlsys.c:sys_get_sysfs"

static const char *extensions[cgr_unknown]=
	{
	[cgr_supervisors]="supervisors",
        [cgr_containers]="containers"
	};

static char extsysfs[200];

const char *sysfs;

if ((sysfs=getenv(VZCGROUP))==(const char *)0) {
  (void) log_alert(0,"%s <%s> env variable missing (config!)",
		      OPEP,VZCGROUP);
  sysfs=SYSVZ;
  }
switch (ext) {
  case cgr_top	:	
    (void) snprintf(extsysfs,sizeof(extsysfs),"%s","/sys/fs/cgroup");
    break;
  case cgr_vzgot	:	
    (void) snprintf(extsysfs,sizeof(extsysfs),"%s",sysfs);
    break;
  case cgr_supervisors	:	//NO BREAK
  case cgr_containers	:
    (void) snprintf(extsysfs,sizeof(extsysfs),"%s/%s",sysfs,extensions[ext]);
    break;
  default		:
    (void) log_alert(0,"%s, Wrong ext; value='%d'  (Bug!!)",OPEP,(int)ext);
    (void) snprintf(extsysfs,sizeof(extsysfs),"/tmp");
    break;
  }
return extsysfs;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to read a single value from	*/
/*	a path (usually a number from /proc	*/
/*	kernel value.				*/
/*						*/
/************************************************/
PUBLIC _Bool sys_read_sys_long(const char *path,u_vlong *result)

{
#define	OPEP	"subprc.c:read_sys_long"

_Bool isok;
char *max;
int fd;
ssize_t n;
char *err;
char buf[30];
int phase;
_Bool proceed;

isok=false;
max="max";
fd=0;
n=0;
err=(char *)0;
(void) memset(buf,'\000',sizeof(buf));
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//checking path
      if ((path==(char *)0)||(result==(u_vlong *)0)) {
        (void) log_alert(0,"%s No path! or result not set (Bug!!)",OPEP);
	phase=999;	//no need to go fruther
	}
      break;
    case 1	:	//opening the file
      if ((fd=open(path,O_RDONLY))<0) {
        (void) log_alert(0,"%s unable to open <%s> (error=<%s> system?)",
			       OPEP,path,strerror(errno));
	phase=999;	//no need to go fruther
	}
      break;
    case 2	:	//reading file
      if ((n=read(fd,buf,sizeof(buf)-1))<0)
        (void) log_alert(0,"%s unable to read from <%s> (error=<%s> system?)",
			    OPEP,path,strerror(errno));
      close(fd);
      if (n<=0) {
        (void) log_alert(0,"%s no data available in <%s>",OPEP,path);
	phase=999;
	}
      break;
    case 3	:	//converting the number to a long
      if (strncmp(buf,max,strlen(max))==0) {
        (void) log_alert(0,"%s <%s> value is '%s', using HOST '%llu' MBytes",
			    OPEP,path,max,*result/(1024*1024));
	isok=true;
	phase=999;
	}
      break;
    case 4	:	//converting the number to a long
      *result=strtoll(buf,&err,10);
      if ((*err!='\000')&&(!isspace((unsigned char)*err))) {
        (void) log_alert(0,"%s from <%s> unable to convert <%s> (system?)",
			    OPEP,path,buf);
	*result=0;	//force result value
	phase=999;
	}
      break;
    case 5	:	//everythin fine
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
/*	Procedure to write a single value to	*/
/*	a path (usually a number stored to	*/
/*	/sysfs/cgroup/"container"/"register"	*/
/*						*/
/************************************************/
PUBLIC _Bool sys_write_sys_long(const char *path,u_vlong tostore)

{
#define	OPEP	"subprc.c:write_sys_long"
_Bool isok;
char buf[30];
int fd;
ssize_t n;
int phase;
_Bool proceed;

isok=true;
fd=0;
n=0;
(void) snprintf(buf,sizeof(buf),"%llu",tostore);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//checking path
      if (path==(char *)0) {
        (void) log_alert(0,"%s No path! (Bug!!)",OPEP);
	phase=999;	//no need to go fruther
	}
      break;
    case 1	:	//opening the file
      if ((fd=open(path,O_WRONLY))<0) {
        (void) log_alert(0,"%s unable to open <%s> (error=<%s> system?)",
			    OPEP,path,strerror(errno));
	phase=999;	//no need to go fruther
	}
      break;
    case 2	:	//writing to file
      if ((n=write(fd,buf,strlen(buf)))<0) {
        (void) log_alert(0,"%s unable to write to <%s> (error=<%s> system?)",
			    OPEP,path,strerror(errno));
	phase=999;
	}
      close(fd);
      break;
    case 3	:	//everything stored?
      if (n<strlen(buf)) {
        (void) log_alert(0,"%s Only stored '%zd' of <%s> to <%s>",
			   OPEP,n,buf,path);
	phase=999;
	}
      break;
    case 4	:	//success at last
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
/*	Procedure to format bandwidth structure	*/
/*	to be displayed on the status template	*/
/*						*/
/************************************************/
PUBLIC const char *sys_bandwidth_printf(BANTYP bandwidth[2],_Bool speed)

{
static char format[60];
int large;
static const char *sec[]={"","/s"};

large=9;
(void) snprintf(format,sizeof(format),"---- Bytes;---- bytes");
if (bandwidth!=(BANTYP *)0) {
  const char *unit[2];
  double res[2];
  int prec[2];
  char tpl[2][15];

  (void) memset(tpl,'\000',sizeof(tpl));
  if (speed==true)
    large++;
  for (int i=0;i<2;i++) {
    u_vlong valeur;

    valeur=bandwidth[i].cumulated;
    if (speed==true) {
      valeur=(u_vlong)(bandwidth[i].speed);
      }
    res[i]=get_unit((double)valeur,&(unit[i]));
    prec[i]=2;
    if (res[i]>=1000.0)  prec[i]--;
    if (res[i]>=10000.0) prec[i]--;
    (void) snprintf(tpl[i],sizeof(tpl[i]),"%7.*f%sB%s",
				  prec[i],res[i],unit[i],sec[speed]);
    }
  (void) snprintf(format,sizeof(format),"%-*s / %-*s",large,tpl[0],large,tpl[1]);
  //to keep result allways same lenght
  //(void) snprintf(format,sizeof(format),"%-21s",loc);
  }
return format;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to move current procedd to	*/
/*	another cgroup.				*/
/*						*/
/************************************************/
PUBLIC _Bool sys_move_to_cgroup(const char *contname,CGRENU ext)

{
#define	OPEP	"utlsys.c:sys_move_to_cgroup"
#define	PROC	"cgroup.procs"

_Bool isok;
FILE *fichier;
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=false;
fichier=(FILE *)0;
ppath[0]='\000';
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//building the right extension
      switch (ext) {
        case cgr_top		:
	  (void) snprintf(ppath,sizeof(ppath),"%s/%s",sys_get_sysfs(ext),PROC);
	  break;
        case cgr_containers	:
        case cgr_supervisors	:
	  (void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",
					      sys_get_sysfs(ext),contname,PROC);
	  break;
        default			:
	  (void) log_alert(0,"%s, Unexpected CGR_ENUM='%d' (Bug!)",OPEP,ext);
	  phase=999;
	  break;
	}
      break;
    case 1	:	//opening the cgroup.procs file
      if ((fichier=fopen(ppath,"w"))==(FILE *)0) {
        (void) log_alert(0,"%s Unable to open file <%s> (error=<%s>)",
                            OPEP,ppath,strerror(errno));
        phase=999;
        }
      break;
    case 2	:	//moving process to another cgroup
      (void) fprintf(fichier,"%d\n",getpid());
      if (fclose(fichier)!=0) {
	(void) log_alert(0,"%s Unable to close file pid='%d' to <%s> (error=<%s>)",
                            OPEP,getpid(),ppath,strerror(errno));
	phase=999;	//proces was NOT moved
	}
      break;
    case 3	:	//move accomplished
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
/*	Procedure to format MEMINFO structure	*/
/*	to be displayed on the status template	*/
/*						*/
/************************************************/
PUBLIC const char *sys_meminf_printf(MEMINF *mems)

{
static char format[60];

(void) snprintf(format,sizeof(format),"----/---- Bytes");
if (mems!=(MEMINF *)0)
  (void) snprintf(format,sizeof(format),"%1.2f/%1.2f %sbytes",
					((double)mems->used)/mems->quotient,
					((double)mems->max)/mems->quotient,
					mems->unit);
return (const char *)format;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to assign cpu to container	*/
/*	Assignation is done via a		*/
/*	"Smart Affinity Schedule"		*/
/*	to make cpu usage evently distributed	*/
/*						*/
/************************************************/
PUBLIC _Bool sys_setcpuset(const char *contname,double ratio)

{
#define	OPEP	"utlsys.c:sys_setcpuset"

_Bool isok;
long maxcpu;
int phase;
_Bool proceed;

isok=false;
maxcpu=0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//how many cpu do we have
      if ((maxcpu=sysconf(_SC_NPROCESSORS_ONLN))<0) {
	(void) log_alert(0,"%s, number of available CPU? (error=<%s> system?)",
			    OPEP,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//select and assigne
      if ((isok=cpuselector(contname,maxcpu,ratio))==false) {
	(void) log_alert(0,"%s Unable to assign CPU to container <%s> Aborting!",
			    OPEP,contname);
	phase=999;	//trouble trouble
	}
      break;
    case 3	:	//Everything fine at last
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
/*	Procedure to retreive the number of	*/
/*	CPU assigned to the container.		*/
/*	Return the number of CPU on the HOST	*/
/*	if unable to retreive container value	*/
/*						*/
/************************************************/
PUBLIC uint16_t sys_get_numcpu(const char *contname)

{
#define	OPEP	"utlsys.c:sys_get_numcpu"
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
      if (fgets(data,sizeof(data),fichier)==(char *)0) {
	(void) log_alert(0,"%s Unable to get data from <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      (void) fclose(fichier);
      break;
    case 2	:	//counting cpu
      //(void) apl_cleanstring(data);	//remove '\n' from line
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
    case 3	:	//making sure everything is right
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
/*	Procedure to retrieve the list of CPUs 	*/
/*	within the HOST (Hardware cpu list)	*/
/*						*/
/************************************************/
PUBLIC _Bool sys_get_cpu_host_list(PHYCPU *cpuslist)

{
#define	OPEP	"utlsys.c:sys_get_cpu_host_list"

_Bool isok;
int phase;
_Bool proceed;

isok=true;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//checking if pointer is OK
     if (cpuslist==(PHYCPU *)0) {
	(void) log_alert(0,"%s Pointer NULL (bug!?)",OPEP);
	phase=999;	//trouble trouble
	}
      break;
    case 1	:	//getting number of Hardware CPU
      (void) memset(cpuslist,'\000',sizeof(PHYCPU));
      if ((cpuslist->maxcpu=sysconf(_SC_NPROCESSORS_CONF))<0) {
	(void) log_alert(0,"%s, %s (error=<%s> system?)",
			   OPEP,"unable to get CPU core number",
			   strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 2	:	//getting number of available CPU
      if ((cpuslist->maxavail=sysconf(_SC_NPROCESSORS_ONLN))<0) {
	(void) log_alert(0,"%s, %s (error=<%s> system?)",
			   OPEP,"unable to get available CPU number",
			   strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 3	:	//getting number of available CPU
      if (sched_getaffinity(0,sizeof(cpu_set_t),&(cpuslist->allocated))<0) {
	(void) log_alert(0,"%s, CPU affinity probe failed (error=<%s> system?)",
			    OPEP,strerror(errno));
	phase=999;	//trouble trouble
	} 
      break;
    case 4	:	//counting the number of cpu
      isok=true;	//hoping for the best
      for (int i=0;i<cpuslist->maxcpu;i++) {
	if (CPU_ISSET(i,&(cpuslist->allocated))!=0)
	  cpuslist->selected++;
	}
      if (cpuslist->maxavail!=cpuslist->selected) {
	(void) log_alert(0,"%s, CPU discrepnncy with cpu numbers, "
			   "avail='%ld' selected='%ld'",
			    OPEP,cpuslist->maxavail,cpuslist->selected);
	isok=false;	//got the worth
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
/*	Procedure to retrieve the list of CPUs 	*/
/*	currently assigned to container		*/
/*						*/
/************************************************/
PUBLIC _Bool sys_get_cpu_cont_list(PHYCPU *cpuslist)

{
_Bool isok;

isok=false;
if (cpuslist!=(PHYCPU *)0) {
  *cpuslist=cont_cpus_list;
  isok=true;
  }
return isok;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure used to prepare kernel to 	*/
/*	activate namespace.			*/
/*						*/
/************************************************/
PUBLIC _Bool  sys_pin_cgroup_ns(pid_t cpid)

{
#define	OPEP	"utlsys.c:sys_pin_cgroup_ns"

_Bool isok;
char ppath[PATH_MAX];
int fd;

isok=false;
snprintf(ppath,sizeof(ppath), "/proc/%d/ns/cgroup",cpid);
// Ouvrir ce fichier force le noyau à lier et valider le cgroup_ns
if ((fd=open(ppath,O_RDONLY|O_CLOEXEC))<0) {
  (void) log_alert(0,"%s can not pin namespace <%s> (error=<%s>)",
			    OPEP,ppath,strerror(errno));
  }
else {
  (void)close(fd);
  isok=true;
  }
return isok;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to set a maximun usage of	*/
/*	available cpu.				*/
/*						*/
/************************************************/
PUBLIC _Bool sys_set_cont_usage(const char *contname,double ratio)

{
#define	OPEP	"utlsys.c:sys_set_cont_usage"
//checking usage every SAMPLE Usec
#define	SAMPLE	100000		

_Bool isok;
uint16_t numcpu;
u_vlong usage;
FILE *fichier;
const char *sysfs;
char ppath[PATH_MAX];
char consigne[30];
int phase;
_Bool proceed;

isok=false;
usage=(u_vlong)0;
fichier=(FILE *)0;
sysfs=sys_get_sysfs(cgr_containers);
(void) snprintf(ppath,sizeof(ppath),"/%s/%s",sysfs,CPUMAX);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//checking if kernel is cpu.max willing
      if (access(ppath,R_OK)<0) {
	(void) log_alert(0,"%s Can not use MAXCPU ratio (file <%s>, error=<%s>)",
			    OPEP,ppath,strerror(errno));
	phase=999;
	}
      break;
    case 1	:	//getting the number of CPU assigned to container
      if ((numcpu=sys_get_numcpu(contname))==0) {
	(void) log_alert(0,"%s No CPU assigned to container! (Bug?!)",OPEP);
	phase=999;
	}
      break;
    case 2	:	//calculate the maximun usage time over sample
      usage=(u_vlong)((ratio*numcpu*SAMPLE)/100.0);
      if (usage<=0)
	usage=SAMPLE/4;
      (void) snprintf(consigne,sizeof(consigne),"%llu %llu",usage,(u_vlong)SAMPLE);
      if (usage>=(numcpu*SAMPLE))
        (void) snprintf(consigne,sizeof(consigne),"max %llu",(u_vlong)SAMPLE);
      break;
    case 3	:	//storing the cpu.max
      (void) snprintf(ppath,sizeof(ppath),"/%s/%s/%s",sysfs,contname,CPUMAX);
      if ((fichier=fopen(ppath,"w"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open file <%s> (error=<%s>)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;	
    case 4	:	//storing cpu max value within cgroup
      (void) fprintf(fichier,"%s\n",consigne);
      (void) fclose(fichier);
      (void) log_alert(0,"%s, container <%s> cpu.max=<%s> (ratio=%5.2f%%)",
			  OPEP,contname,consigne,ratio);
      isok=true;
      break;	
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	SAMPLE
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to return the cpu set 	*/
/*	assigned to a container.		*/
/*						*/
/************************************************/
PUBLIC char *sys_get_cont_cpulist(const char *contname)

{
#define	OPEP	"utlsys.c:sys_get_cont_cpulist"

char *list;
FILE *fichier;
const char *sysfs;
char ppath[PATH_MAX];
char line[200];
int phase;
_Bool proceed;

list=(char *)0;
fichier=(FILE *)0;
line[0]='\000';
sysfs=sys_get_sysfs(cgr_containers);
(void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",sysfs,contname,"cpuset.cpus");
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Opening the container cpuset file
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//deep deep trouble
	}
      break;
    case 1	:	//reading file contents
      if (fgets(line,sizeof(line),fichier)==(char *)0) {
	(void) log_alert(0,"%s file <%s> contents empty! (system?)",OPEP,ppath);
	phase=999;	//deep trouble trouble
	}
      (void) fclose(fichier);
      break;
    case 2	:	//parsing list
      list=calloc(strlen(line)+1,sizeof(char));
      if (sscanf(line,"%s\n",list)!=1) {
	(void) log_alert(0,"%s file <%s> unable to parse <%s> (system?)",
			    OPEP,ppath,line);
	(void) free(list);
	list=(char *)0;
	phase=999;	//deep trouble trouble
	} 
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return list;

#undef	CPUSET
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to set the container weight	*/
/*	value.					*/
/*						*/
/************************************************/
PUBLIC _Bool sys_set_cont_weight(const char *contname,double ratio)

{
#define	OPEP	"utlsys.c:sys_set_cont_weight"
#define	MAXW	10000		//cgroup V2 limit

_Bool isok;
u_vlong total_weight;
FILE *fichier;
const char *sysfs;
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=true;
total_weight=0;
fichier=(FILE *)0;
sysfs=sys_get_sysfs(cgr_containers);
(void) snprintf(ppath,sizeof(ppath),"/%s/%s/%s",sysfs,"",WEIGHT);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Reading the assigned the crgoup weight
      if ((isok=sys_read_sys_long(ppath,&total_weight))==false) {
	(void) log_alert(0,"%s Unable to get data from <%s> (Config?)",
			    OPEP,ppath);
	phase=999;	//No need to go further
	}
      break;
    case 1	:	//adjusting the weight according valeur ratio
      total_weight=(u_vlong)((total_weight*ratio)/100.0);
      if (total_weight<1)
	total_weight = 1; 
      if (total_weight>MAXW)
	total_weight=MAXW;
      break;
    case 2	:	//adjusting the weight according valeur ratio
      (void) snprintf(ppath,sizeof(ppath),"/%s/%s/%s",sysfs,contname,WEIGHT);
      if ((fichier=fopen(ppath,"w"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open file <%s> (error=<%s>)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 3	:	//writing the new weight
      (void) fprintf(fichier,"%llu\n",total_weight);
      (void) fclose(fichier);
      (void) log_alert(0,"%s, container <%s> cpu.weight='%llu' (ratio=%5.2f%%)",
			  OPEP,contname,total_weight,ratio);
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	MAXW
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to get container/Host weight	*/
/*	ratio.					*/
/*						*/
/************************************************/
PUBLIC char *sys_get_cont_weight(const char *contname)

{
#define	OPEP	"utlsys.c:sys_get_weight_ratio"

char *weight;
u_vlong total_weight;
u_vlong cont_weight;
const char *sysfs;
char ppath[PATH_MAX];
int phase;
_Bool proceed;

weight=(char *)0;
total_weight=0;
cont_weight=0;
sysfs=sys_get_sysfs(cgr_containers);
ppath[0]='\000';
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Reading the assigned the crgoup weight
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",sysfs,WEIGHT);
      if (sys_read_sys_long(ppath,&total_weight)==false) {
	(void) log_alert(0,"%s Unable to get data from <%s> (Config?)",
			    OPEP,ppath);
	phase=999;	//No need to go further
	}
      break;
    case 1	:	//adjusting the weight according valeur ratio
      (void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",sysfs,contname,WEIGHT);
      if (sys_read_sys_long(ppath,&cont_weight)==false) {
	(void) log_alert(0,"%s Unable to get data from <%s> (error=<%s>)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 2	:	//writing the new weight
      (void) snprintf(ppath,sizeof(ppath),"%llu/%llu",cont_weight,total_weight);
      weight=strdup(ppath); 
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return weight;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to get container CPUs usage	*/
/*	ceiling.				*/
/*						*/
/************************************************/
PUBLIC char *sys_get_cont_ceiling(const char *contname)

{
#define	OPEP	"utlsys.c:sys_get_ceiling"

char *ceiling;
const char *sysfs;
FILE *fichier;
char ppath[PATH_MAX];
char line[100];
char l1[50];
u_vlong l2;
int phase;
_Bool proceed;

ceiling=(char *)0;
sysfs=sys_get_sysfs(cgr_containers);
fichier=(FILE *)0;
(void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",sysfs,contname,CPUMAX);
line[0]='\000';
l1[0]='\000';
l2=(u_vlong)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Opening the container cpu.max file
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//deep deep trouble
	}
      break;
    case 1	:	//reading ceilling value
      if (fgets(line,sizeof(line),fichier)==(char *)0) {
	(void) log_alert(0,"%s Unable to read <%s> contents (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//deep deep trouble
	}
      (void) fclose(fichier);
      break;
    case 2	:	//reading ceilling value
      if (sscanf(line,"%31s %llu",l1,&l2)!=2) {
	(void) log_alert(0,"%s Unable to scan <%s> contents (bug?)",OPEP,line);
	phase=999;	//deep deep trouble
	}
      break;
    case 3	:	//formating result
      (void) snprintf(line,sizeof(line),"%s/%llu",l1,l2);
      ceiling=strdup(line);
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return ceiling;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to get container CPUs		*/
/*	statistic.				*/
/*						*/
/************************************************/
PUBLIC _Bool sys_get_cpustat(CPUENU cpuenu,STATYP *status)

{
#define	OPEP	"utlsys.c:sys_get_cpustat"
#define	CPTS	15
#define PRESFMT	"some avg10=%*f avg60=%*f avg300=%*f total=%llu\n"
#define	FIELD	"some"


_Bool isok;
int count;
int collected;
FILE *fichier;
const char *sysfs;
CPUINF *cpuinf;
char lines[CPTS][100];
char keys[CPTS][50];
char spath[PATH_MAX-100];
char ppath[PATH_MAX];
u_vlong values[CPTS];
int phase;
_Bool proceed;

isok=false;
count=0;
collected=0;
fichier=(FILE *)0;
sysfs=sys_get_sysfs(cgr_containers);
(void) memset(lines,'\000',sizeof(lines));
(void) memset(keys,'\000',sizeof(keys));
switch (cpuenu) {
  case cpu_host		:
    (void) snprintf(spath,sizeof(spath),"%s",SYSFS);
    break;
  case cpu_cont		:
    (void) snprintf(spath,sizeof(spath),"%s/%s",sysfs,status->contname);
    break;
  case cpu_unknown	:	//no break
  default		:
    (void) log_alert(0,"%s cpuenu index='%d is out of range (Bug! exiting)",
			OPEP,cpuenu);
    (void) exit(1);
    break;
  }
cpuinf=&(status->cpuinf[cpuenu]);
(void) snprintf(ppath,sizeof(ppath),"%s/%s",spath,CPUSTAT);
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Opening the container cpu.max file
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//deep deep trouble
	}
      break;
    case 1	:	//reading cpu.stat values
      while (fgets(lines[collected],sizeof(lines[collected]),fichier)!=(char *)0) {
	collected++;
	if (collected>=CPTS) {
	  (void) log_alert(0,"%s Lines[%d]; array overflow (%s format?)",
			    OPEP,count,CPUSTAT);
	  phase=999;	//file too large
	  break;
	  }
	}
      (void) fclose(fichier);
      break;
    case 2	:	//parsing cpu.stat values
      for (int i=0;i<collected;i++) {
        if (sscanf(lines[i],"%49[^ ] %llu",keys[i],&(values[i]))!=2) {
	  (void) log_alert(0,"%s unable to parse line[%d]=<%s> from file <%s> (%s)",
			    OPEP,i,lines[i],ppath,"systemd?");
	  phase=999;	//no purpose to go further
	  break;	//exiting scanning loop
	  }
	}
      break;
    case 3	:	//storing values
      for (int i=0;i<collected;i++) {
	//Caution vocsta order mater!
	static const char *vocstat[]={
		"usage_usec",
		"nr_throttled",
		"throttled_usec"
		};

	const char **ptr;
	size_t lim;

	if (lines[i][0]=='\000')
	  break;	//no more lines to find
	ptr=vocstat;
	lim=sizeof(vocstat)/sizeof(char *);
        for (int j=0;j<lim;j++,ptr++) {
  	  if (strcmp(keys[i],*ptr)==0) {
	    count++;
	    switch(j) {
      	      case 0	:	//usage_usec
	        cpuinf->usage=values[i];
	        break;
              case 1	:	//nr_throttled
	        cpuinf->nr_throttled=(u_int)values[i];
	        break;
      	      case 2	:	//throttled_usec
	        cpuinf->throttled=values[i];
	        break;
	      }
	    break;	//exiting search loop
	    }
	  }
	if (count==lim) {	//all data collected
	  isok=true;
	  break;
	  }
	}
      break;
    case 4	:	//collecting pressure
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",spath,CPUPRES);
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?, Kernel?)",
			    OPEP,ppath,strerror(errno));
        if (access(ppath,F_OK)!=0)
          (void) log_alert(0,"%s: DIAGNOSTIC - PSI interface missing. %s",
                              OPEP,"Verify 'psi=1' is in your "
                                    "kernel boot parameters.");
        else
          (void) log_alert(0,"%s: DIAGNOSTIC - File exists but cannot be read. %s",
                              OPEP,"Check permissions/AppArmor/SELinux.");
	(void) log_alert(0,"%s Aborting processus)",OPEP);
	phase=999;	//deep deep trouble
	}
      break;
    case 5	:	//reading pressure information
      while (fgets(lines[0],sizeof(lines[0]),fichier)!=(char *)0) {
	if (strncmp(lines[0],FIELD,strlen(FIELD))==0) {
	  int n;

	  if ((n=sscanf(lines[0],PRESFMT,&(cpuinf->pressure)))!=1) {
	    (void) log_alert(0,"%s scan of <%s> returned '%d' (%s)",
				OPEP,lines[0],n,"Unexpected format?");
	    phase=999;  //trouble trouble
            }
          break;        //no need to scan further
          }
        }
      (void) fclose(fichier);
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	FIELD
#undef	CPTS
#undef	PRESFMT
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to compute how much is the cpu*/
/*	idle.					*/
/*						*/
/************************************************/
PUBLIC double sys_get_cpu_idle(STATYP *status)

{
struct timeval tv;
u_vlong working;

(void) gettimeofday(&tv,(struct timezone *)0);
working=(((tv.tv_sec*1000000ULL)+tv.tv_usec)-status->start)*status->numcpu;
return (double)((working-status->cpuinf[cpu_cont].usage)*100)/working;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to set the current time within*/
/*	file to be available inside the		*/
/*	container 'homefs'/infos/ directory, the*/
/*	file is named 'starting'.		*/
/*						*/
/************************************************/
PUBLIC _Bool sys_set_start(pid_t contpid)

{
#define	OPEP	"utlsys.h:sys_set_start"

_Bool isok;
FILE *fichier;
const char *homefs;
u_vlong start;
struct timeval tv;
char ppath[PATH_MAX];
int phase;
_Bool proceed;

isok=false;
fichier=(FILE *)0;
homefs=getenv(HOMEFS);
(void) gettimeofday(&tv,(struct timezone *)0);
start=(tv.tv_sec*1000000ULL)+tv.tv_usec;
ppath[0]='\000';
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d' ppath=<%s>",OPEP,phase,ppath);
  switch (phase) {
    case 0	:	//checking if we have homefs
      if ((homefs==(char *)0)||(homefs[0]=='\000')) {
	(void) log_alert(0,"%s missing homefs (Config? Bug!)",OPEP);
	phase=999;
	}
      break;
    case 1	:	
      (void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",homefs,INFODIR,STARTFS);
      if ((fichier=fopen(ppath,"w"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//no need to go further
	}
      break;
    case 2	:	//writing starting time
      isok=true;
      (void) fprintf(fichier,"%d %llu\n",contpid,start);
      if (fclose(fichier)!=0) {
	(void) log_alert(0,"%s Unable to close <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	isok=false;
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
/*	Procedure to get the current time within*/
/*	file available inside the contaner 	*/
/*	'homefs'/infos/ directory, the file is	*/
/*	named 'starting'.			*/
/*	return 0 if not successful.		*/
/*						*/
/************************************************/
PUBLIC _Bool sys_get_start(pid_t *contpid,u_vlong *start)

{
#define	OPEP	"utlsys.h:sys_get_start"

_Bool isok;
FILE *fichier;
const char *homefs;
char ppath[PATH_MAX];
char line[100];
int phase;
_Bool proceed;

isok=false;
fichier=(FILE *)0;
*start=(u_vlong)0;
*contpid=(pid_t)0;
homefs=getenv(HOMEFS);
ppath[0]='\000';
line[0]='\000';
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d' ppath=<%s>",OPEP,phase,ppath);
  switch (phase) {
    case 0	:	//checking if we have homefs
      if ((homefs==(char *)0)||(homefs[0]=='\000')) {
	(void) log_alert(0,"%s missing homefs (Config? Bug!)",OPEP);
	phase=999;
	}
      break;
    case 1	:	
      (void) snprintf(ppath,sizeof(ppath),"%s/%s/%s",homefs,INFODIR,STARTFS);
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s Unable to open <%s> (error=<%s> system?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//no need to go further
	}
      break;
    case 2	:	//writing starting time
      if (fgets(line,sizeof(line),fichier)==(char *)0) {
	(void) log_alert(0,"%s Unable to read <%s> file contents (config? Bug!?)",
			    OPEP,ppath);
	phase=999;	//no need to go further
	}
      (void) fclose(fichier);
      break;
    case 3	:	//reading value
      isok=true;
      if (sscanf(line,"%d %llu",contpid,start)!=2) {
	(void) log_alert(0,"%s Unable to parse line <%s> (config? Bug!?)",
			    OPEP,line);
	isok=false;
	phase=999;	//no need to go further
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
/*	Procedure to return the host loadavg	*/
/*	values.					*/
/*						*/
/************************************************/
PUBLIC _Bool sys_get_host_loadavg(double *avgs,u_int taille)

{
#define OPEP    "utlsys.c:prc_get_host_loadavg"

_Bool isok;
FILE *fichier;
char ppath[PATH_MAX];
char data[200];
int phase;
_Bool proceed;

isok=false;
(void) memset(avgs,'\000',sizeof(double)*taille);
fichier=(FILE *)0;
(void) snprintf(ppath,sizeof(ppath),"/proc/loadavg");
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //Getting the container data
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
        (void) log_alert(0,"%s Unable to open HOST loadavg <%s> (error=<%s> %s)",
			    OPEP,ppath,strerror(errno),"system?");
        phase=999;      //trouble trouble
        }
      break;
    case 1	:	//Reading data file  
      if (fgets(data,sizeof(data),fichier)==(char *)0) {
        (void) log_alert(0,"%s Unable to get data from <%s> (error=<%s> %s?)",
                            OPEP,ppath,strerror(errno),"system?");
        phase=999;      //trouble trouble
        }
      (void) fclose(fichier);
      break;
    case 2	:	//scaning data
      isok=true;	//assume the best
      if ((sscanf(data,"%lf %lf %lf",avgs,avgs+1,avgs+2))!=3) {
        (void) log_alert(0,"%s Unable to scan data <%s> (Bug?)",OPEP,data);
        isok=false;     //best is not always true
        phase=999;
        }
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
/*	Procedure calculate the load average	*/
/*	accorind the delta_t whith previous	*/
/*	procedure call				*/
/*						*/
/************************************************/
PUBLIC _Bool sys_cal_loadavg(STATYP *status)

{
#define	OPEP	"utlsys.c:sys_cal_loadavg"
#define	MINPACE	0.01	//minimun space time calculation (10 ms)
#define ZEROAVG "?.??  ?.??  ?.?? 0/0 0"

static char loadavg[40]=ZEROAVG;
static u_vlong last_load[cpu_unknown];

_Bool isok;
u_vlong cur_load[cpu_unknown];
double host_avg[3];
double ratio;
int phase;
_Bool proceed;

isok=false;
(void) snprintf(loadavg,sizeof(loadavg),"%s",ZEROAVG);
status->loadavg=loadavg;
for (CPUENU c=cpu_host;c<cpu_unknown;c++)
   cur_load[c]=last_load[c];
(void) memset(host_avg,'\000',sizeof(host_avg));
ratio=0.0;
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d'",OPEP,phase);
  switch (phase) {
    case 0	:	//computing current load
      for (CPUENU c=cpu_host;c<cpu_unknown;c++)
        cur_load[c]=status->cpuinf[c].usage+status->cpuinf[c].pressure;
      if (last_load[cpu_host]==(u_vlong)0) 
	phase=999;		//wait for next time
      break;
    case 1	:	//computing delta_t
      if (status->delta_t<MINPACE) 
        phase=999;      //No!; no need to update
      break;
    case 2	:	//computing ratio container/HOST
      if (status->delta_t>=MINPACE) {	//always
	double delta_cpu[cpu_unknown];

        for (CPUENU c=cpu_host;c<cpu_unknown;c++)
          delta_cpu[c]=cur_load[c]-last_load[c];
	if (delta_cpu[cpu_host]>0.0) {	//should always be the case
	  ratio=delta_cpu[cpu_cont]/delta_cpu[cpu_host];
	  if (ratio>1.0)
	    ratio=1.0;
	  break;
	  }
	if (ratio<0.0) {
          (void) log_alert(0,"%s Beware load ratio='%f' (expected>0.0 Bug?)",
			     OPEP,ratio);
	  phase=999;	//trouble
	  }
	}
      break;
    case 3	:	// Computing ratio container/HOST
      if (sys_get_host_loadavg(host_avg,3)==false) {
	(void) log_alert(0,"%s Unable to get HOST current load (Bug?)",OPEP);
	phase=999;
	}
      break;
    case 4	:	// extracting last_pid
      if (get_last_pid(status)==false) 
	phase=999;	// No last_pid??
      if (cnt_pids_current(status)==false)
	phase=999;	// No PIDs_current??
      break;
    case 5	:	//applying ratio
      for (int i=0;i<sizeof(host_avg)/sizeof(double);i++)
        host_avg[i]*=ratio;
      (void) snprintf(loadavg,sizeof(loadavg),"%5.2lf %5.2lf %5.2lf 1/%-4u %-4u",
                                               host_avg[0],
                                               host_avg[1],
                                               host_avg[2],
					       status->pids_current,
					       status->last_pid);
      isok=true;
      break;
    default	:	//SAFE Guard
      for (CPUENU c=cpu_host;c<cpu_unknown;c++)
        last_load[c]=cur_load[c];
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	ZEROAVG
#undef	MINPACE
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure create a status structure and	*/
/*	set initial value (as start,pid,name..) */
/*						*/
/************************************************/
PUBLIC STATYP *sys_new_cont_status()

{
#define	OPEP	"utlsys.c:sys_new_cont_status"

STATYP *status;
char *ptr;
int phase;
_Bool proceed;

status=(STATYP *)calloc(1,sizeof(STATYP));
(void) sys_get_start(&(status->contpid),&(status->start));
ptr=(char *)0;
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d'",OPEP,phase);
  switch (phase) {
    case 0	:	//set container name
      if ((ptr=getenv(NAME))==(char *)0) {
	(void) log_alert(0,"%s Unable to get container name! (config?)",OPEP);
	status=sys_free_cont_status(status);
	phase=999;	//no need to go further
	}
      break;
    case 1	:	//ta
      status->delta_t=0.0;
      if (clock_gettime(CLOCK_MONOTONIC,&(status->last_time))<0) {
         (void) log_alert(0,"%s Unable to get MONOTONIC clock! (error=<%s> %s)",
                            OPEP,strerror(errno),"system?");
	status=sys_free_cont_status(status);
        phase=999;      //We are in real real trouble. exiting at once
        }
      break;
    case 2	:	//set container name
      status->contname=strdup(ptr);
      status->cpumodel=strdup(get_cpu_model_name());
      status->cpuonl=sysconf(_SC_NPROCESSORS_ONLN);
      status->numcpu=sys_get_numcpu(status->contname);
      status->cpuassigned=sys_get_cont_cpulist(status->contname);
      status->ceiling=sys_get_cont_ceiling(status->contname);
      status->weight=sys_get_cont_weight(status->contname);
      for (CPUENU c=cpu_host;c<cpu_unknown;c++) {
	char name[field_sizeof(NETINF,intname)];
	char *ref;
	char *ip_num;

	ip_num=(char *)0;
	ref=status->network[c].ip_num;
	switch (c) {
	  case cpu_host	:
	    (void) snprintf(name,sizeof(name),"%s",getenv(BRIDGENAME));
	    ip_num=get_ip_num(name);
	    break;
	  case cpu_cont	:
	    (void) snprintf(name,sizeof(name),"v-%s",status->contname);
	    ip_num=ns_get_ip_num(status->contpid,getenv(ETHNAME));
	    break;
	  default	:
	    (void) snprintf(name,sizeof(name),"Undefined?!");
	    break;
	  }
	(void) snprintf(ref,sizeof(((NETINF *)0)->ip_num),"%s","Unknown!");
	if (ip_num!=(char *)0) {
	  (void) snprintf(ref,sizeof(((NETINF *)0)->ip_num),"%s",ip_num);
	  (void) free(ip_num);
	  }
	ref=status->network[c].intname;
	(void) snprintf(ref,sizeof(((NETINF *)0)->intname),"%s",name);
        (void) sys_get_cpustat(c,status);
	}
      (void) sys_cal_loadavg(status);
      (void) usleep(10000);	//10 ms relax time
      status->delta_t=0.011;
      (void) sys_cal_loadavg(status);
      break;
    default	:	//SAFE Guard
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
/*	Procedure to update status structure	*/
/*	with fresh data.			*/
/*						*/
/************************************************/
PUBLIC _Bool sys_update_cont_status(STATYP *status)

{
#define	OPEP	"utlsys.c:sys_update_cont_status"

_Bool isok;
TIMETIC cur_time;
int phase;
_Bool proceed;

isok=false;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//very unlikely
      if (status==(STATYP *)0) {
	(void) log_alert(0,"%s Missing status pointer (Bug!)",OPEP);
	phase=999;	//no need to go further
	}
      break;
    case 1	:	//computing delta_t
      status->delta_t=0.0;
      if (clock_gettime(CLOCK_MONOTONIC,&cur_time)<0) {
         (void) log_alert(0,"%s Unable to get MONOTONIC clock! (error=<%s> %s)",
                            OPEP,strerror(errno),"system?");
        phase=999;      //We are in real real trouble. exiting at once
        }
      break;
    case 2	:	//computing delta_t
      status->delta_t=(double)(cur_time.tv_sec-status->last_time.tv_sec)+
              	       (double)((cur_time.tv_nsec-status->last_time.tv_nsec)/1e9);
      status->last_time=cur_time;
      break;
    case 3	:	//get the number of connect user
      if (update_users(status->contpid,&(status->nbruser))==false) {
	(void) log_alert(0,"%s Unable to get number of container users (%s)",
			    OPEP,"System?");
	phase=999;	//no need to go further
	}
      break;
    case 4	:	//get all memory information
      for (CPUENU c=cpu_host;c<cpu_unknown;c++) {
        (void) get_memory_info(status,c,mem_mem);
        (void) get_memory_info(status,c,mem_swap);
        (void) get_memory_stress(status,c);
	}
      break;
    case 5	:	//get the container cpu stats information
      (void) sys_get_cpustat(cpu_host,status);
      (void) sys_get_cpustat(cpu_cont,status);
      status->cpuidle=sys_get_cpu_idle(status);
      (void) sys_cal_loadavg(status);
      break;
    case 6	:	//get both Host and container traffic value
      (void) get_bandwidth_info(&(status->network[cpu_host]),status->delta_t);
      (void) get_bandwidth_info(&(status->network[cpu_cont]),status->delta_t);
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
/*	Procedure free memory used by a status	*/
/*	structure.				*/
/*						*/
/************************************************/
PUBLIC STATYP *sys_free_cont_status(STATYP *status)

{
if (status!=(STATYP *)0) {
  if (status->weight!=(char *)0)
    (void) free(status->weight);
  if (status->cpumodel!=(char *)0)
    (void) free(status->cpumodel);
  if (status->ceiling!=(char *)0)
    (void) free(status->ceiling);
  if (status->cpuassigned!=(char *)0)
    (void) free(status->cpuassigned);
  if (status->contname!=(char *)0)
    (void) free(status->contname);
  (void) free(status);
  status=(STATYP *)0;
  }
return status;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure initiate the daemon random	*/
/*	sees.					*/
/*						*/
/************************************************/
PUBLIC _Bool sys_set_random_seed()

{
#define	OPEP	"utlsys.c:sys_set_random_seed"

_Bool isok;
TIMETIC cur_time;
int phase;
_Bool proceed;

isok=false;
cur_time.tv_sec=0;
cur_time.tv_nsec=0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//gettime real time
      if (clock_gettime(CLOCK_MONOTONIC,&cur_time)<0) {
        (void) log_alert(0,"%s Unable to get MONOTONIC clock! (error=<%s> system?)",
                            OPEP,strerror(errno));
	(void) srand((unsigned int)time(NULL));
        phase=999;
        }
      break;
    case 1	:	//gettime real tim
      (void) srand((unsigned int)cur_time.tv_nsec);
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
