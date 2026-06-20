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
/*	Define debug level to define logs	*/
/*	routines				*/
/*						*/
/************************************************/
#ifndef DBGLOG
#define	DBGLOG

#include	<stdarg.h>
#include	<stdbool.h>

#ifdef VZGOT_DBG
extern const char* __asan_default_options(void);
#endif

//application debug level
extern int debug;

/*application working mode foreground/background*/
extern _Bool foreground;

/*application verbosity level			*/
extern int verbose;

//to display message on console (verbose mode) or
//via syslog (LOG_DAEMON) using variable argument list macros
void log_valert(const int dlevel,const char *fmt,va_list ap)
		__attribute__((format(printf,2,0)));

//to display message on console (verbose mode) or
//via syslog (LOG_DAEMON)
extern void log_alert(const int dlevel,const char *fmt,...)
		__attribute__((format(printf,2,3)));

#endif
