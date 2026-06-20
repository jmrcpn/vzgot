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
/*	Define utility level procedure to handle*/
/*	process function.			*/
/*						*/
/************************************************/
#ifndef SUBPRC
#define SUBPRC
#include	<sched.h>
#include	<stdarg.h>
#include	<stdbool.h>
#include	<stdint.h>

#include	"lowtyp.h"

//PORC CONFIG variable
#define	MEMMAX	"MEMMAX"
#define	SWAPMAX	"SWAPMAX"
#define	PIDMAX	"PIDMAX"

/*Signal to be send to check if a process is	*/
/*still up and running				*/
#define SIGCHECK	0

//container info file structure
typedef	void *INFPTR;

typedef	struct	{
	u_int nbr;	/*nbr process in queue	*/
	pid_t *pqueue;	/*process queue		*/
	}PRCTYP;

extern int prc_checkprocess(pid_t pid);
extern void prc_nozombie();
extern PRCTYP *prc_addtoprc(PRCTYP *prclst,pid_t pid);
extern PRCTYP *prc_childkill(PRCTYP *prclst,int delay);
extern PRCTYP *prc_childcheck(PRCTYP *prclst);

//procedure to return the number of current pids related to container
extern _Bool prc_cnt_pids_current(const char *contname,uint32_t *pids_current);

//procedure to return a container cpu usage 
extern _Bool prc_cnt_usage(const char *contname,u_vlong *usage);

//procedure to return a container pressure total
extern _Bool prc_cnt_pressure(const char *contname,u_vlong *usage);

//procedure to set the cgroup pids.max value to container
extern _Bool prc_setpidsmax(const char *contname,const char *valeur);

//to set the memory size available to container
extern _Bool prc_setmemmax(const char *contname,const char *valeur,_Bool swap);

//to get the memory size available to container
extern _Bool prc_getcgroupmem(const char *contname,const char *mname,unsigned long long *cgmem);

//to get the HOST (hardware) load average 
extern _Bool prc_host_loadavg(double *avg60,double *avg300,double *avg900);

//procedure to get container last pid (used by cal_loadavg)
extern _Bool prc_get_last_pid(const char *contname,uint32_t *last_pid);

//procedute to get the number of users related to a container PID
extern _Bool prc_upd_users(pid_t contpid,u_int *nbrusr);

#endif
