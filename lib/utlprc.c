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
/*	SUBPRC:					*/
/*	take care of all low level process	*/
/*	handling.				*/
/*						*/
/************************************************/
#include	<sys/resource.h>
#include	<sys/prctl.h>
#include	<sys/time.h>
#include	<ctype.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<signal.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<unistd.h>

#include	"dbglog.h"
#include	"lowtyp.h"
#include	"utlvec.h"
#include	"subprc.h"

#define	DEVNULL	"/dev/null"

/*Process command override structure (proc)	*/
typedef	struct	{
	int max;	/*title max size	*/
	char **newargv;	/*newly set argv list	*/
	char *title;	/*title storage area	*/
	}TITLTYP;

/*storage area for /proc title display		*/
static  TITLTYP *title=(TITLTYP *)0;
/*

*/
/************************************************/
/*						*/
/*	Procedure to catch a SIGALRM signal	*/
/*						*/
/************************************************/
static void trppace(int sig)

{
#define	OPEP	"subprc.c:trppace,"

static u_long tics=(u_long)0;

switch (sig) {
  case SIGALRM   :
    tics++;
    break;
  default        :
    (void) fprintf(stderr,"%s Unexpected signal <%s> received\n",
			  OPEP,strsignal(sig));
    (void) fflush(stderr);
    break;
  }

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to free used title memory	*/
/*						*/
/************************************************/
char **prc_cleantitle()

{
if (title!=(TITLTYP *)0) {
  title->newargv=(char **)vec_freelstlst((void **)title->newargv,VECFREE);
  if (environ!=(char **)0) {
    int i;

    for (i=0;environ[i]!=(char *)0;i++) {
      (void) free(environ[i]);
      environ[i]=(char *)0;
      }
    (void) free(environ);
    environ=(char **)0;
    }
  (void) free(title);
  title=(TITLTYP *)0;
  }
return (char **)0;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to find and limit space to	*/
/*	be used as status information available	*/
/*	from proc (ps ax)			*/
/*						*/
/************************************************/
char **prc_preptitle(int argc,char *argv[],char *env[])

{
extern char **environ;

char *lastend;

lastend=(char *)0;
(void) prc_cleantitle();
title=(TITLTYP *)calloc(1,sizeof(TITLTYP));
if (argv!=(char **)0) {
  int i;

  title->title=argv[0];
  title->newargv=(char **)vec_addlstlst((void **)title->newargv,(void *)strdup(argv[0]));
  for (i=1;argv[i]!=(char *)0;i++) {
    char *valeur;

    lastend=argv[i]+strlen(argv[i]);
    valeur=strdup(argv[i]);
    title->newargv=(char **)vec_addlstlst((void **)title->newargv,(void *)valeur);
    argv[i]=(char *)0;
    }
  }
if (env!=(char **)0) {
  int i;

  environ=(char **)0;
  for (i=0;env[i]!=(char *)0;i++) {
    char *valeur;

    lastend=env[i]+strlen(env[i]);
    valeur=strdup(env[i]);
    (void) putenv(valeur);
    }
  }
title->max=lastend-title->title;
return title->newargv;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to update title information.	*/
/*	title information is available via the	*/
/*	"ps" command.				*/
/*						*/
/************************************************/
void prc_settitle(const char *fmt,...)

{
va_list args;

va_start(args,fmt);
if ((title!=(TITLTYP *)0)&&(title->title!=(char *)0)) {
  (void) bzero(title->title,title->max);
  (void) vsnprintf(title->title,title->max,fmt,args);
  }
va_end(args);
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to put a process in background*/
/*	mode.					*/
/*						*/
/************************************************/
void prc_divedivedive(_Bool live,const char *appname)

{
#define	OPEP	"subprc.c:prc_divedivedive,"

if (live==false) {
  switch (fork()) {
    case -1	:
      (void) fprintf(stderr,"%s, Unable to dive! (error=<%s>)",
			     OPEP,strerror(errno));
      /*lets continue if foreground mode		*/
      break;
    case  0	:
      //we are now in background mode
      //live is always false so lest do a seuid
      (void) setsid();
      break;
    default	:
      /*waiting for ballast to fill up :-}}	*/
      (void) sleep(1);	
      (void) exit(0);	/*just exit		*/
      break;		/*never reached		*/
    }
  }

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to start or stop a 'Pacemaker'*/
/*	within the application.			*/
/*						*/
/************************************************/
void prc_pace(u_long millisec,int onoff)

{
int static done=false;
struct sigaction oldsa;

if (onoff==true) {
  if (done==false) {
    struct sigaction newsa;
    struct itimerval period;

    newsa.sa_flags=0;
    newsa.sa_handler=trppace;
    (void) sigemptyset(&newsa.sa_mask);
    (void) sigaction(SIGALRM,&newsa,&oldsa);
    period.it_value.tv_sec=millisec/1000;
    period.it_value.tv_usec=millisec*1000;
    period.it_interval.tv_sec=period.it_value.tv_sec;
    period.it_interval.tv_usec=period.it_value.tv_usec;
    (void) setitimer(ITIMER_REAL,&period,(struct itimerval *)0);
    done=true;
    }
  }
else {
  if (done==true) {
    struct itimerval period;

    period.it_value.tv_sec=0;
    period.it_value.tv_usec=0;
    period.it_interval.tv_sec=period.it_value.tv_sec;
    period.it_interval.tv_usec=period.it_value.tv_usec;
    (void) setitimer(ITIMER_REAL,&period,(struct itimerval *)0);
    (void) sigaction(SIGALRM,&oldsa,(struct sigaction *)0);
    done=false;
    }
  }
}
