/************************************************/
/*						*/
/*      Copyright:				*/
/*	 Jean-Marc Pigeon <jmp@safe.ca>	 2026	*/
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
/*	Implement a very sub level procedure to */
/*	logs application events			*/
/*						*/
/************************************************/
#include	<stdio.h>
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
if ((foreground==true)&&(verbose==true))
  (void) fprintf(stderr,"%s%s\n",lvl,strloc);
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
