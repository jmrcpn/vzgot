/************************************************/
/*						*/
/*      Copyright:				*/
/*	 Jean-Marc Pigeon <jmp@safe.ca>	 2018	*/
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
/*	Define utility level procedure to load	*/
/*	application environement/configuration.	*/
/*						*/
/************************************************/
#ifndef UNICFG
#define UNICFG

#include	<stdbool.h>
#include	<stdio.h>

//Directory where container supervisor drop
//live data
#define	INFODIR	"infos"

//ENV variable names 
#define	ONLINETPL	"ONLINETPL"	//the "online" template

//Reference to the online template
typedef	void *TPLPTR;

extern TPLPTR *cfg_open_online(const char *confdir,const char *contname,const char *tplname);
extern _Bool cfg_update_online(const char *contname,TPLPTR *tplptr);
extern _Bool cfg_flush_online(FILE *dest,TPLPTR *tplptr);
extern TPLPTR *cfg_close_online(TPLPTR *tplptr);
#endif
