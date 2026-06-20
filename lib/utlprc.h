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
#ifndef UTLPRC
#define UTLPRC

#include	<unistd.h>
#include	<stdlib.h>
#include	<stdarg.h>

extern char **prc_cleantitle();
extern char **prc_preptitle(int argc,char *argv[],char *env[]);
extern void prc_settitle(const char *fmt,...);
extern void prc_divedivedive(_Bool live,const char *appname);

extern void prc_pace(u_long millisec,int onoff);

#endif
