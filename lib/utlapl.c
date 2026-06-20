/************************************************/
/*						*/
/*      Copyright:				*/
/*		 Jean-Marc Pigeon <jmp@safe.ca>	*/
/*	Distributed under the Gnu Public	*/
/*	License, see the License file in this	*/
/*	package.				*/
/*						*/
/*	Implement very sub level procedure to 	*/
/*	handle clement specific needs.		*/
/*						*/
/************************************************/
#include	<sys/time.h>
#include	<sys/stat.h>
#include	<sys/resource.h>
#include	<sys/prctl.h>
#include	<ctype.h>
#include	<errno.h>
#include	<limits.h>
#include	<malloc.h>
#include	<signal.h>
#include	<sched.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<stdint.h>
#include	<string.h>
#include	<syslog.h>
#include	<time.h>
#include	<unistd.h>
#include	"dbglog.h"
#include	"lowtyp.h"
#include	"version.h"
#include	"utlapl.h"

/*sigterm request flag				*/
	int sigterm=false;
/*sigquit request flag				*/
	int sigquit=false;
/*sigint request flag container HALT		*/
	int sigint=false;
/*sigint request flag container REBOOT		*/
	int sighup=false;
/*sigcont request flag container to start	*/
	int sigcont=false;

/*application name				*/
	char	*appname=VZGOT;
//pending signal activiy and count
static volatile sig_atomic_t signal_counts[_NSIG]; 
static volatile sig_atomic_t signal_pending[_NSIG];

/*

*/
/************************************************/
/*						*/
/*	Procedure to catch signal and do what is*/
/*	needed.					*/
/*						*/
/************************************************/
static void trapsig(int sig)

{
signal_counts[sig]++;
//(void) log_alert(0,"trapsig, JMPDBG received signal=%d' <%s>",sig,strsignal(sig));
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to catch signal and do what is*/
/*	needed.					*/
/*						*/
/************************************************/
static void trpmempbls(int sig)

{
#define	OPPB	"subclm.c:trpmempbls,"

switch (sig) {
  case SIGSEGV   :
    (void) apl_core_dump("Program genuine memory violation");
    break;
  default        :
    (void) apl_core_dump("%s Unexpected signal <%s> received",
			 OPPB,strsignal(sig));
    break;
  }
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to display an core_dump	*/
/*	message and terminate application	*/
/*						*/
/************************************************/
static void core_dump(char *crashdir,char *temps,const char *fmt,va_list args)

{
#define MXSTEP 30

va_list ap;
struct rlimit limites;

va_copy(ap,args);
if (getrlimit(RLIMIT_CORE,&limites)<0) {
  (void) fprintf(stderr,"getrlimit error='%s'",strerror(errno));
  }
limites.rlim_cur=limites.rlim_max;
if (setrlimit(RLIMIT_CORE,&limites)<0) {
  (void) fprintf(stderr,"setrlimit error='%s'",strerror(errno));
  }
(void) prctl(PR_SET_DUMPABLE,1,0,0,0);/*to allow core-dump	*/
if (chdir(crashdir)<0) {
  char command[2000];
  char dirdump[1000];

  (void) snprintf(dirdump,sizeof(dirdump),"%s/coredump/","/tmp");
  (void) snprintf(command,sizeof(command),"mkdir -p %s",dirdump);
  if (system(command)<0) {
    (void) fprintf(stderr,"Unable to execute command <%s> (error=<%s>)\n",
		           command,strerror(errno));
    }
  if (chdir(dirdump)<0) {
    (void) abort();
    }
  crashdir=dirdump;
  }
if (temps!=(char *)0)
  (void) fprintf(stderr,"%s ",temps);
(void) vfprintf(stderr,fmt,ap);
(void) fprintf(stderr,"\n");
(void) fprintf(stderr,"going to CORE DUMP (in %score.%d)\n",
		       crashdir,getpid());
(void) fflush(stderr);
(void) sleep(5);	/*to avoid crash avalanche	*/
(void) abort();		/*doing the abort		*/
(void) exit(-1);	/*Theoriticaly unreachable	*/
}
/*

*/
/********************************************************/
/*                                                      */
/*	Procedure to dump the current process memory	*/
/*	state via syslog. This routine is activated	*/
/*	under SIGPWR signal reception.			*/
/*                                                      */
/********************************************************/
PUBLIC void apl_mem_report(void) 

{
struct mallinfo2 mi;
FILE *fichier;

//Recovering memory statistique
mi=mallinfo2();
(void) log_alert(0,"--- MEMORY REPORT (SIGPWR) ---");
// Mémoire totale allouée via sbrk (Heap classique)
(void) log_alert(0,"Heap main arena   : %zu bytes",mi.arena);
// Nombre de "chunks" libres
(void) log_alert(0,"Free chunks       : %zu",mi.ordblks);
// Mémoire allouée via mmap (souvent les gros blocs ou librairies)
(void) log_alert(0,"Mmap regions      : %zu (%zu bytes)",mi.hblks,mi.hblkhd);
// Total de la mémoire activement utilisée par l'application
(void) log_alert(0,"Total allocated   : %zu bytes",mi.uordblks);
// Total de la mémoire libre dans les arènes
(void) log_alert(0,"Total free        : %zu bytes",mi.fordblks);
// Le "Keepcost" : quantité de mémoire libérable par malloc_trim()
(void) log_alert(0,"Releasable (trim) : %zu bytes", mi.keepcost);
if ((fichier=fopen("/proc/self/statm","r"))!=(FILE *)0) {
  uint64_t pages;

  if (fscanf(fichier,"%*d %ju",&pages)==1)  // On saute le 1er, on lit le 2e
    (void) log_alert(0,"Kernel RSS : %ld bytes",pages*sysconf(_SC_PAGESIZE)); 
  (void) fclose(fichier);
  }
(void) log_alert(0,"------------------------------");
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to check if directory is	*/
/*	already existing.			*/
/*						*/
/************************************************/
int apl_isdir(char *dirpath)

{
int status;
struct stat bufstat;

status=-1;
if (stat(dirpath,&bufstat)==0) {
  if (S_ISDIR(bufstat.st_mode)!=0) {
    status=0;
    }
  }
return status;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to free memory used by a	*/
/*	string, do not proceed if point is NULL.*/
/*						*/
/************************************************/
char *apl_freestr(char *str)

{
if (str!=(char *)0) {
  (void) free(str);
  }
return (char *)0;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to extract and return current	*/
/*	version number.				*/
/*						*/
/************************************************/
const char *apl_getvers()

{
return VERSION"."RELEASE;
}
/*
^L
*/
/************************************************/
/*						*/
/*      Procedure to return the current time	*/
/*	expressed with a millisecond precision.	*/
/*						*/
/************************************************/
u_long apl_getmillisec()

{
static time_t start=(time_t)0;

u_long millisec;
struct timeval newtime;

millisec=(u_long)0;
(void) gettimeofday(&newtime,(struct timezone *)0);
if (start==(time_t)0)
  start=(newtime.tv_sec-1);
millisec=newtime.tv_sec-start;
millisec*=1000;
millisec+=(newtime.tv_usec/1000);
return millisec;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to make create a unique name	*/
/*	can be used to store process data	*/
/*	name return is dynamically allocated.	*/
/*						*/
/************************************************/
char *apl_uniquename(unsigned int seq)

{
#define UFTIME	"%Y%m%d%H%M%S"
#define	UNIQUE	"%05d-%s-%08lx"

char *uniquename;
time_t curtime;
struct tm *tminfo;
char asctemps[100];

curtime=time((time_t)0);
tminfo=localtime(&curtime);
uniquename=(char *)calloc(1,NAME_MAX);
(void) strftime(asctemps,sizeof(asctemps),UFTIME,tminfo);
(void) snprintf(uniquename,NAME_MAX,UNIQUE,getpid(),asctemps,apl_getmillisec()+seq);
return uniquename;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to return a time seen as local*/
/*	time to a long express as YYYYMMDD	*/
/*						*/
/************************************************/
PUBLIC u_long apl_date(time_t curtime)

{
struct tm *tm;

tm=localtime(&curtime);
return (tm->tm_year*10000)+(tm->tm_mon*100)+tm->tm_mday;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to return the uptime as a 	*/
/*	string computed as the current time	*/
/*	minus the start time			*/
/*						*/
/************************************************/
PUBLIC char *apl_uptime(char *uptime,int taille,u_vlong start)

{
struct timeval tv;
u_vlong sec;

uptime[0]='\000';
(void) gettimeofday(&tv,(struct timezone *)0);
start/=1000000ULL;
if (tv.tv_sec>=start) {
  unsigned long reste;
  unsigned long jours;
  unsigned long heures;
  unsigned long minutes;
  unsigned long secondes;
  sec=tv.tv_sec-start;
  jours=sec/86400;
  reste=sec%86400;
  heures=reste/3600;
  reste%=3600;
  minutes=reste/60;
  secondes=reste%60;
  (void) snprintf(uptime,taille,"% 3ld days %02ld:%02ld:%02ld",
			         jours,heures,minutes,secondes);
  }
return uptime;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to return a duration in ms as	*/
/*	day:hours:min:sec.ms format		*/
/*						*/
/************************************************/
PUBLIC char *apl_chrono_ms(char *chrono,int taille,u_vlong ms)

{
u_vlong sec;
unsigned long reste;
unsigned long jours;
unsigned long heures;
unsigned long minutes;
unsigned long secondes;

chrono[0]='\000';
sec=ms/1000;
ms%=1000;
jours=sec/86400;
reste=sec%86400;
heures=reste/3600;
reste%=3600;
minutes=reste/60;
secondes=reste%60;
(void) snprintf(chrono,taille,"% 3ld days %02ld:%02ld:%02ld.%03lld",
                                 jours,heures,minutes,secondes,ms);
return chrono;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to return the local time in	*/
/*	HH:MM:SS format.			*/
/*	Time is stored in a STATIC memeory area	*/
/*						*/
/************************************************/
char *apl_ascsystime(time_t curtime)

{
#define TTIME	"%H:%M:%S"

static char asctemps[100];

struct tm *tminfo;

tminfo=localtime(&curtime);
(void) strftime(asctemps,sizeof(asctemps),TTIME,tminfo);
return asctemps;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to return system time from an	*/
/*	YYYY-MM-DDD HH:MM:SS format.		*/
/*						*/
/************************************************/
time_t apl_datetimesysasc(char *strdate,char *strtime)

{
#define	DBDTOUNIX	"%Y-%m-%d %H:%M:%S"
time_t datetime;

datetime=(time_t)0;
if ((strdate!=(char *)0)&&(strtime!=(char *)0)) {
  struct tm tm;
  char strdt[100];

  (void) memset(&tm,'\000',sizeof(struct tm));
  tm.tm_isdst=-1;
  (void) snprintf(strdt,sizeof(strdt),"%s %s",strdate,strtime);
  if (strptime(strdt,DBDTOUNIX,&tm)!=(char *)0) {
    datetime=mktime(&tm);
    }
  else {
    (void) log_alert(0,"subclm.c:apl_datetimesysasc Unable to convert <%s> to time_t",strdt);
    }
  }
return datetime;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to return the local time in	*/
/*	YYYY-MM-DDD format.			*/
/*	date is stored in a STATIC memeory area	*/
/*						*/
/************************************************/
char *apl_ascsysdate(time_t curtime)

{
#define TDATE	"%Y-%m-%d"

static char asctemps[100];

struct tm *tminfo;

tminfo=localtime(&curtime);
(void) strftime(asctemps,sizeof(asctemps),TDATE,tminfo);
return asctemps;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to return the local time in	*/
/*	YYYY-MM-DDD HH:MM:SS format.		*/
/*	date is stored in a STATIC memeory area	*/
/*						*/
/************************************************/
char *apl_ascsysdatetime(time_t curtime)

{
#define TDTIME	"%Y-%m-%d %H:%M:%S"

static char asctemps[100];

struct tm *tminfo;

tminfo=localtime(&curtime);
(void) strftime(asctemps,sizeof(asctemps),TDTIME,tminfo);
return asctemps;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to transform the local system	*/
/*	time in ASCII time stamp.		*/
/*	Stored in STATIC memory area.		*/
/*						*/
/************************************************/
char *apl_ascsysstamp(time_t curtime)

{
#define TSTAMP	"%a, %d %b %Y %H:%M:%S"

static char ascstamp[100];

struct tm *tminfo;
char asct[80];

tminfo=localtime(&curtime);
(void) strftime(asct,sizeof(asct),TSTAMP,tminfo);
(void) snprintf(ascstamp,sizeof(ascstamp),"%s %05ld",asct,
					  ((tminfo->tm_gmtoff/3600)*100)+
					  ((tminfo->tm_gmtoff%3600)/60));
return ascstamp;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to display an argv list 	*/
/*						*/
/************************************************/
void apl_argvtrace(const int dlevel,const char *fmt,char *argv[])

{
#define	ZONE	500

if ((debug>=dlevel)&&(argv[0]!=(char *)0)) {
  register int i;
  char strtmp[ZONE];

  (void) snprintf(strtmp,sizeof(strtmp),"%s",argv[0]);
  for (i=1;argv[i]!=(char *)0;i++) {
    (void) strncat(strtmp," ",(ZONE-1)-strlen(strtmp));
    (void) strncat(strtmp,argv[i],(ZONE-1)-strlen(strtmp));
    }
  (void) log_alert(dlevel,fmt,strtmp);
  }

#undef	ZONE
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to 'compute' an application 	*/
/*	directory according dir enum value	*/
/*						*/
/************************************************/
char *apl_appdir(DIRENUM dir)

{
char *appdir;
char *sysbase;
char *apvers;
char *subdir;

sysbase=(char *)0;
appdir=(char *)0;
apvers=apl_getapvers();
subdir="";
switch (dir) {
  case (d_null)		:
    sysbase="";
    apvers=""; 
    break;
  case (d_tmp)		:
    sysbase="/var/tmp";
    apvers=VZGOT"/"; 
    break;
  case (d_crash)	:
    sysbase="/var/crash";
    break;
  case (d_etc)		:
    sysbase="/etc";
    apvers=VZGOT"/"; 
    break;
  case (d_spool)	:
    sysbase="/var/spool";
    break;
  case (d_lock)		:
    sysbase="/run";
    apvers=VZGOT"/";
    break;
  case (d_vzgot)	:
    sysbase="/var/lib";
    apvers=VZGOT;
    subdir="/vzdir";	
    break;
  case (d_log)		:
    sysbase="/var/spool";
    subdir="logs";
    break;
  case (d_ubin)		:
    sysbase="/usr/bin";
    apvers=""; 
    break;
  case (d_usbin)	:
    sysbase="/usr/sbin";
    apvers=""; 
    break;
  case (d_varlib)	:
    sysbase="/var/lib";
    apvers=VZGOT; 
    break;
  case (d_usrlib)	:
    sysbase="/usr/lib";
    break;
  case (d_libexec)	:
    sysbase="/usr/libexec";
    apvers=VZGOT; 
    break;
  default		:
    /*something impossible !?		*/
    break;
  }
if (sysbase!=(char *)0) {
  int taille;

  taille=strlen(sysbase);
  taille+=strlen(apvers);
  taille+=strlen(subdir);
  taille+=4;	//spare space
  appdir=(char *)calloc(taille,sizeof(char)); 
  (void) snprintf(appdir,taille,"%s/%s%s",sysbase,apvers,subdir);
  }
return appdir;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to set the SIGV signal trap	*/
/*	purpose is to CORE_DUMP in case we have	*/
/*	a memory failure of some kind.		*/
/*						*/
/************************************************/
void apl_trapsegv(int onoff)

{
static struct sigaction oldsa;

if (onoff==true) {
  struct sigaction newsa;

  newsa.sa_flags=0;
  newsa.sa_handler=trpmempbls;
  (void) sigemptyset(&newsa.sa_mask);
  (void) sigaction(SIGSEGV,&newsa,&oldsa);
  }
else {
  (void) sigaction(SIGSEGV,&oldsa,(struct sigaction *)0);
  }
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to prepare a core_dump in	*/
/*	the right directory.			*/
/*						*/
/************************************************/
void apl_core_dump(char *frmt,...)

{
va_list args;
char *crashdir;
char *temps;

va_start(args,frmt);
temps=apl_ascsystime(time((time_t *)0));
crashdir=apl_appdir(d_crash);
(void) core_dump(crashdir,temps,frmt,args);
va_end(args);
}
/*

*/
/************************************************/
/*						*/
/*	procedure to set/unset trapped signal	*/
/*						*/
/************************************************/
void apl_settrap(int set)

{
#define	OPEP	"subapl.c:apl_settrap"

static struct sigaction oldint;
static struct sigaction oldhup;
static struct sigaction oldterm;
static struct sigaction oldquit;
static struct sigaction oldusr1;
static struct sigaction oldusr2;
static struct sigaction oldpwr;
static struct sigaction oldchild;
static struct sigaction oldcont;

static int alldone=false;

if (set==alldone) {
  switch (set) {
    case true	:
      (void) apl_core_dump("apl_settrap already set");
      break;
    case false	:
      (void) apl_core_dump("apl_settrap not previously set");
      break;
    default	:
      (void) apl_core_dump("apl_settrap unproper set value");
      break;
    } 
  }
(void) apl_trapsegv(set);
if (set==true) {
  TIMETIC notimeout;
  struct sigaction newsa;
  int count;

  notimeout.tv_sec=0;
  notimeout.tv_nsec=0;
  count=0;
  (void) sigemptyset(&newsa.sa_mask);
  (void) sigaddset(&newsa.sa_mask,SIGCHLD);
  //purging sigchld allready in queue
  while (sigtimedwait(&newsa.sa_mask,(siginfo_t *)0,&notimeout)>0) {
    if (count>10) {
      (void) log_alert(0,"%s, Waiting too long for empty signal queue (Bug?)",
			  OPEP);
      break;
      }
    (void) sched_yield();
    count++;
    }
  (void) sigfillset(&newsa.sa_mask);
  newsa.sa_flags=0;
  newsa.sa_handler=trapsig;
  (void) sigaction(SIGCONT,&newsa,&oldcont);
  (void) sigaction(SIGPWR,&newsa,&oldpwr);
  (void) sigaction(SIGCHLD,&newsa,&oldchild);
  (void) sigaction(SIGUSR2,&newsa,&oldusr2);
  (void) sigaction(SIGUSR1,&newsa,&oldusr1);
  (void) sigaction(SIGINT,&newsa,&oldint);
  (void) sigaction(SIGHUP,&newsa,&oldhup);
  (void) sigaction(SIGTERM,&newsa,&oldterm);
  (void) sigaction(SIGQUIT,&newsa,&oldquit);
  }
else {
  (void) sigaction(SIGQUIT,&oldquit,(struct sigaction *)0);
  (void) sigaction(SIGTERM,&oldterm,(struct sigaction *)0);
  (void) sigaction(SIGHUP,&oldhup,(struct sigaction *)0);
  (void) sigaction(SIGINT,&oldint,(struct sigaction *)0);
  (void) sigaction(SIGUSR1,&oldusr1,(struct sigaction *)0);
  (void) sigaction(SIGUSR2,&oldusr2,(struct sigaction *)0);
  (void) sigaction(SIGCHLD,&oldchild,(struct sigaction *)0);
  (void) sigaction(SIGPWR,&oldpwr,(struct sigaction *)0);
  (void) sigaction(SIGCONT,&oldcont,(struct sigaction *)0);
  }
alldone=set;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to set a string to lower case	*/
/*	only.					*/
/*						*/
/************************************************/
char *apl_strtolower(char *str)

{
if (str!=(char *)0) {
  register char *ptr;

  for (ptr=str;*ptr!='\000';ptr++)
    *ptr=(char)tolower((int)*ptr);
  }
return str;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure extract line from a file,	*/
/*	forget about any line starting with	*/
/*	carcom character (if carcom character	*/
/*	not null).				*/
/*	Doesn't return the '\r' and '\n' 	*/
/*	character.				*/
/*						*/
/************************************************/
char *apl_getstr(FILE *fichier,char *str,u_int taille,char carcom)

{
char *strloc;

(void) memset(str,'\000',taille);
while ((strloc=fgets(str,taille,fichier))!=(char *)0) {
  char *ptrloc;
  int len;

  if (carcom!='\000') {
    if (str[0]==carcom) {
      str[0]='\000';	//clearing line contents
      break;		//returning an empty line
      }
    ptrloc=str;
    while ((ptrloc=strchr(ptrloc,carcom))!=(char *)0) {
      if ((ptrloc!=str)&&(*(ptrloc-1)=='\\')) {
	(void) memmove(ptrloc-1,ptrloc,strlen(ptrloc)+1);
        ptrloc++;
        }
      else {
        *ptrloc='\000';
        break;
        }
      }
    }
  len=strlen(strloc);
  ptrloc=strloc+len;
  while (len>0) {
    len--;
    switch (*ptrloc) {
      case '\n'     :
      case '\r'     :
        *ptrloc='\000';
        break;
      default       :
	len=0;
	break;
      }
    ptrloc--;
    }
  break;
  }
return strloc;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to application+version name	*/
/*						*/
/************************************************/
char *apl_getapvers()

{
static char *apvers=(char *)0;

if (apvers==(char *)0) {
  static char apinfo[50];

  char *ptr;
  char version[30];

  (void) strcpy(version,apl_getvers());
  if ((ptr=strchr(version,'-'))!=(char *)0)
    *ptr='\000';
  (void) snprintf(apinfo,sizeof(apinfo),"%s-%s/",appname,version);
  apvers=apinfo;
  }
return apvers;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to convert a string to a code,*/
/*	remaining of the string is copy at the	*/
/*	beginning.				*/
/*	Return a pointer to table, never a NULL	*/
/*	pointer unless the table itself is NULL	*/
/*						*/
/************************************************/
VOCTYP *apl_getvoca(const VOCTYP *table,char *str)

{
VOCTYP *voc;

voc=(VOCTYP *)0;
if (table!=(VOCTYP *)0) {
  int i;
  char *sptr;

  sptr=str; 
  while (isspace(*sptr)!=0)
    sptr++;
  for (i=0;table[i].key!=(char *)0;i++) {
    int max;

    max=strlen(table[i].key);
    if (strncasecmp(table[i].key,sptr,max)==0) {
      if ((sptr[max]!='\000')&&(isalnum(sptr[max])!=0))
        continue;/*it is not the right word	*/
      sptr+=max;
      while (isspace(*sptr)!=0)
        sptr++;
      (void) memmove(str,sptr,strlen(sptr)+1);
      voc=(VOCTYP *)(table+i);
      break;
      }
    }
  if (voc==(VOCTYP *)0)
   voc=(VOCTYP *)(table+i);/*return the "unknown" info	*/
  }
return voc;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to remove crlf and space AT	*/
/*	the string end.				*/
/*	Remove space at the string start too.	*/
/*						*/
/************************************************/
char *apl_cleanstring(char *str)

{
if (str!=(char *)0) {
  register int taille;
  register char *ptr;

  taille=strlen(str); 
  ptr=str+taille-1;
  while (taille>0) {
    if ((*ptr=='\n') || (*ptr=='\r') || (*ptr==' ')) {
      *ptr='\000';
      ptr--;
      taille--;
      continue;
      }
    break;
    }
  ptr=str;
  while (isblank(*ptr)!=0)
    ptr++;
  if (str!=ptr)
    (void) memmove(str,ptr,strlen(ptr)+1);
  }
return str;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to delay action, this		*/
/*	procedure is for debug only purpose	*/
/*	(ex: you want ito slow dow some part of	*/
/*	excution process..), It should never	*/
/*	be called when the package is in	*/
/*	production.				*/
/*						*/
/************************************************/
unsigned int apl_dodelay(unsigned int delais)

{
unsigned int remaining;

remaining=delais;
if (delais>0) {
  (void) log_alert(0,"DEBUG, requesting a '%d' sec delay",delais);
  remaining=sleep(delais);
  (void) log_alert(0,"DEBUG, exiting delais with remaining='%u' sec",remaining);
  }
return remaining;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to check signal received	*/
/*	 Return the number of signal received	*/
/*						*/
/************************************************/
PUBLIC int apl_checksig()

{
#define	OPEP	"subapl.c:apl_checksig"

int count;

count=0;
for (int i=0;i<_NSIG;i++) {
  if (signal_counts[i]==0)
    continue;
  count+=signal_counts[i];
  if (signal_counts[i]>1)
    (void) log_alert(0,"%s multiple '%d' signal '%s' pending!",
			OPEP,signal_counts[i],strsignal(i));
  signal_counts[i]=0;
  switch (i) {
    case SIGINT		:
      sigint=true;
      break;
    case SIGHUP		:
      sighup=true;
      break;
    case SIGQUIT	:
      sigquit=true;
      sigterm=true;
      break;
    case SIGCHLD	:
      (void) log_alert(0,"%s child process stopped or terminated",OPEP);
      break;
    case SIGUSR1	:
      debug++;
      (void) log_alert(0,"%s Increased debug level to '%d'",OPEP,debug);
      break;
    case SIGUSR2	:
      if (debug>0) 
        debug--;
      (void) log_alert(0,"%s debug level is not set to '%d'",OPEP,debug);
      break;
    case SIGTERM	:
      sigterm=true;
      break;
    case SIGPWR		:
      (void) apl_mem_report();
      break;
    case SIGCONT	:
      sigcont=true;
      break;
    default		:
      (void) log_alert(0,"%s Unexpected  signal '%d' (system?)",OPEP,i);
      break;
    }
  }
return count;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to retrieve a double from	*/
/*	string.					*/
/*						*/
/************************************************/
PUBLIC double apl_getdouble(const char *valeur)

{
#define	OPEP	"subapl.c:apl_getdouble"

double convert;
char *err;

err=(char *)0;
convert=strtod(valeur,&err);
if ((*err!='\000')&&(!isspace((unsigned char)*err))) {
  (void) log_alert(0,"%s value=<%s> is not a float ",OPEP,valeur);
  convert=0.0;
  }
return convert;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to  return the default config	*/
/*	full path				*/
/*						*/
/************************************************/
PUBLIC const char *apl_dfltconfdir()

{
static char dflt[PATH_MAX-50]="";

if (dflt[0]=='\000') {
  char *pathetc;

  pathetc=apl_appdir(d_etc);
  (void) snprintf(dflt,sizeof(dflt),"%s",pathetc);
  pathetc=apl_freestr(pathetc);
  }
return dflt;
}
