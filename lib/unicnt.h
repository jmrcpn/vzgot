// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*	Define all routine to handle container	*/
/*	access.			                */
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

//procedure to assign a 'working root user' to container
extern _Bool cnt_mapcontids(const char *contname,pid_t clonepid,uid_t contuid,gid_t contgid);

//procedure to update monitoring /proc file
extern _Bool cnt_updateproc(STATYP *contstat);

//Procedure to set the container PID within the container working space
extern _Bool cnt_set_cont_pid(const char *contname,pid_t cpid);

//Procedure to retreive the container PID within the container working space
extern pid_t cnt_get_cont_pid(const char *contname);

//Procedure to remove the container PID within the container working space
extern int cnt_rm_cont_pid(const char *contname);

extern _Bool cnt_initscript(const char *scriptname,const char *fmt,...);
extern int cnt_injectcmd(const char *scriptname,char *contname,int contpid,char *params);

//procedure to fetch the container working CPU arch
extern const char *cnt_getarch(char *contname);

//procedure to fetch the container distribution template
extern const char *cnt_getdist(char *contname);

extern void cnt_setstdio(_Bool live,char *contname,char *outname);

//Waiting for container console information
//extern int cnt_mstconsole(char *contname,pid_t cntpid,int cconsole);

//to unmount all supervisor devices
extern _Bool cnt_unset_sup_devices(const char *contname);

//procedure to make all need device available to supervisor
//to be shared with container
extern _Bool cnt_set_sup_devices(const char *contname);

//to unmount all container utilities directory (/run,/sys,/dev/,proc)
extern _Bool cnt_unset_rootfs_dir(const char *contname);

//procedure to make sur all special directory within container
//rootfs are available
extern _Bool cnt_set_rootfs_dir(const char *rootfs);

//Procedure to set the cgroup limits within the container
extern _Bool cnt_set_cgroup(const char *contname);

//Procedure to create the container infos tmps area
extern _Bool cnt_create_infos();

//procedure to do a proper container container preparation
extern _Bool cnt_launch(char *contname);

//procedure to do a complete container clean exit
extern _Bool cnt_exit(pid_t cpid,char *contname);

//procedure to do extract the monitoring process PID
extern pid_t cnt_get_monitoring_pid(pid_t cont_pid);

#endif
