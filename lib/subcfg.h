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
#ifndef SUBCFG
#define SUBCFG

#include	<stdbool.h>
#include	<stdio.h>

#include	"utlsys.h"

typedef	struct	{
	char	lavg[50];	//container load average
	char	name[50];	//container name
	pid_t	pid;		//container pid
	u_vlong	start;		//container starting time
	}INFTYP;

//loading application configuration
extern int cfg_loadconfig(const char *confdir,const char *contname);

//procedure te free the info structure
extern INFTYP *cfg_freeinfos(INFTYP *inftyp);

//procedure to create a brand new info structure with static data
extern INFTYP *cfg_init_infos(const char *contname,pid_t cpid);

#endif
