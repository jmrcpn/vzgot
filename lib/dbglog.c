// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*	Implement a very sub level procedure to */
/*	logs application events			*/
/*						*/
/************************************************/
#include	<stdio.h>
#include	<time.h>
#include	<string.h>
#include	<syslog.h>

#include	"dbglog.h"

//debug compilation level
#ifdef VZGOT_DBG
#define TRALOG  LOG_DEBUG

const char* __asan_default_options(void)

{
return "detect_leaks=1:print_stats=1";
}

#else
#define TRALOG  LOG_INFO
#endif

//current debug level
int debug=0;

//application flag running background
_Bool foreground=false;

/*debug log in verbose mode			*/
	int verbose=false;
/*

*/
/********************************************************/
/*							*/
/*	Subroutine to log event on syslog               */
/*							*/
/********************************************************/
PUBLIC void log_valert(const int dlevel,const char *fmt,va_list ap)

{
#define	STRMAX	1000
#define	DEBMAX	 140

char lvl[10];
char strloc[STRMAX];

(void) snprintf(lvl,sizeof(lvl),"(dl=%02d) ",dlevel);
(void) memset(strloc,'\000',sizeof(strloc));
(void) vsnprintf(strloc,sizeof(strloc)-1,fmt,ap);
if (foreground==true) {
  if (verbose==true)
    (void) fprintf(stderr,"%s%s\n",lvl,strloc);
  else {
    char time_str[64];
    struct tm time_info;
    time_t curtime;

    (void) snprintf(time_str,sizeof(time_str),"%s","Date/time?");
    curtime=time((time_t *)0);
    if (localtime_r(&curtime,&time_info)!=(struct tm *)0) 
      (void)strftime(time_str,sizeof(time_str),"%F %T",&time_info);
    (void) fprintf(stderr,"%s %s%s\n",time_str,lvl,strloc);
    }  
  }
else {
  char *ptr;
  int taille;

  ptr=strloc;
  taille=strlen(ptr);
  while (taille>DEBMAX) {
    (void) syslog(TRALOG,"%s%.*s",lvl,DEBMAX,ptr);
    ptr +=DEBMAX;
    taille-=DEBMAX;
    lvl[0]='\000';
    } 
  if (strlen(ptr)>0)
    (void) syslog(TRALOG,"%s%s",lvl,ptr);
  }

#undef	DEBMAX
#undef	STRMAX
}
/*

*/
/********************************************************/
/*							*/
/*	Subroutine to log event on syslog               */
/*							*/
/********************************************************/
PUBLIC void log_alert(const int dlevel,const char *fmt,...)

{
if (debug>=dlevel) {
  va_list args;

  va_start(args,fmt);
  (void) log_valert(dlevel,fmt,args);
  va_end(args);
  }
}
