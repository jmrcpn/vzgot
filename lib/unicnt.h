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
/*	Define all routine to handle container	*/
/*	struture and access.			*/
/*						*/
/************************************************/
#ifndef	UNICNT
#define UNICNT

#include	<stdbool.h>
#include	<stdint.h>

//environement container privileged mode setting
#define	PRIVILEGED	"PRIVILEGED"

#define	ROOTFS	"rootfs"	//Container main root directory
#define	VRUN	"/run/vzgot/%s/"//HOST vproc container directory
#define	VPROC	"vproc"		//HOST dedicated directory

extern	_Bool	privileged;	//Container is working in privileged mode

//procedure to compute container own load average
extern const char *cal_loadavg(const char *contname,uint16_t nbr_cpu,double delta_t);

//Procedure for clone to wait some time to receive a "good" ID
extern _Bool cnt_wait_goodid(int delay,uid_t uid,gid_t gid);

//procedure to assign a 'working root user' to container
extern _Bool cnt_mapcontids(pid_t clonepid,uid_t contuid,gid_t contgid);

//procedure used by container "main" process to make
//sure to be set with right IDs.
extern _Bool cnt_wait_for_mapuser(uid_t contuid,gid_t contgid);

extern _Bool cnt_pivot(char *contname,int cloneflgs);
extern int cnt_setclonepid(char *contname,pid_t cpid);
extern pid_t cnt_getclonepid(char *contname);
extern int cnt_rmclonepid(char *contname);
extern _Bool cnt_initscript(const char *scriptname,const char *fmt,...);
extern int cnt_injectcmd(const char *scriptname,char *contname,int contpid,char *params);
extern char *cnt_getarch(char *contname);
extern char *cnt_getdist(char *contname);
extern void cnt_setstdio(_Bool live,char *contname,char *outname);

//Opening the container console FIFO
extern int cnt_open_cont_console(const char *contname);

//Waiting for container console information
extern int cnt_mstconsole(char *contname,pid_t cntpid,int cconsole);

//Procedure to set the cgroup limits within the container
extern _Bool cnt_setcgroup(const char *contname);

//Procedure to create the container infos tmps area
extern _Bool cnt_create_infos();

//procedure to do a proper container container preparation
extern _Bool cnt_launch(char *contname);

//procedure to do a complete container clean exit
extern _Bool cnt_exit(pid_t cpid,char *contname);

#endif
