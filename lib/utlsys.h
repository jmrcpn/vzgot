/************************************************/
/*						*/
/*	Define utility level procedure to handle*/
/*	CPU fonction				*/
/*						*/
/************************************************/
#ifndef SUBCPU
#define SUBCPU
#include	<sys/types.h>
#include	<sched.h>
#include	<stdint.h>

#include	"lowtyp.h"

//CPU CONFIG variable
#define	NUMCPU	"NUMCPU"
#define	MAXCPU	"MAXCPU"
#define	PWRCPU	"PWRCPU"

#define	INFODIR	"infos"		//container live data directory

//extensions definition por /sys/fs/crgroup/vzgot
typedef enum	{
	cgr_top,		//cggroup TOP /sys/fs/cgroup
	cgr_vzgot,		//cgroup vzgot top level
	cgr_supervisors,	//cgroup supervisors	
	cgr_containers,		//cgroup containers	
	cgr_unknown		//undefined
	}CGRENU;

//structure to keep curent container cpu assignation
typedef	struct	{
	long maxcpu;		//number max of Hardware CPU
	long maxavail;		//Max number of connected CPU
	long selected;		//Number of CPU selected
	cpu_set_t allocated;
	}PHYCPU;

//working type Hos/Container
typedef	enum	{
	cpu_host,		//Host information
	cpu_cont,		//Container cpu information
	cpu_unknown		//sentinel
	}CPUENU;

//memory type (ram/swap)
typedef	enum	{
	mem_mem,		//real memory
	mem_swap,		//really memory
	mem_unknown		//sentinel
	}MEMENU;


//CPUs cgroup information
typedef	struct	{
	u_vlong	usage;		//CPU usage in micro-sec
	u_vlong	pressure;	//CPU pressure in micro-sec
	u_vlong throttled;	//Throttled time in micro-sec
	u_int	nr_throttled;	//Throttled number time
	}CPUINF;

//memory information
typedef	struct	{
	u_vlong	max;		//Maximun memory avalable
	u_vlong used;		//Memory really used
	u_vlong quotient;	//Memory quotient
	char	unit[20];	//Unit used to compute
	}MEMINF;

//Memory stress information 
typedef	struct	{
	double	avg10;		//Memory pressure
	u_vlong	oom_kill;	//Number of process killed
	}OOMINF;

//RX or TX channel inormation
typedef	struct	{
	u_vlong	cumulated;	//accumulated number of bytes
	double	speed;		//speed
	}BANTYP;

//Network information
typedef	struct	{
	char	intname[20];	//Network interface name
	char	ip_num[20];	//Network interface IP num
	BANTYP	traffic[2];	//traffic bytes [received,transmitted]
	}NETINF;

//container status structure
typedef	struct	{
	u_vlong	start;		// Container starting date in usec
	TIMETIC last_time;	// Last time status was updated
	double  delta_t;	// Delta-t with last mesure
	pid_t	contpid;	// Container pid
	u_int	nbruser;	// Number of user connected
	u_int	cpuonl;		// number of cpu online (Host)
	u_int	numcpu;		// Number of cpu assigned to container
	pid_t	last_pid;	// Container last assigned pid
	u_int	pids_current;	// Container nomber of current PIDs
	double	cpuidle;	// %time of cuper doing nothin
	char 	*contname;	// Container name
	char	*cpumodel;	// Host CPU model
	char	*cpuassigned;	// Container list of cpu assigned
	char	*ceiling;	// Container maxcpu Ceiling
	char	*weight;	// Powercpu weight ratio
	const char *loadavg;	// Container load average
	MEMINF	mems[2][2];	// All memories information
	OOMINF  oom[2];		// Memory stress information
	CPUINF	cpuinf[2];	// CPU information for host or container
	NETINF	network[2];	// Host, Container network information	
	}STATYP;

//to display namespace owner ship
extern void sys_show_namespace();

//to detach descripteur vhen a chlild is open
extern _Bool sys_detach_from_supervisor(char *contname);

//to get the SYSFS value
extern const char *sys_get_sysfs(CGRENU ext);

//to read a value from system
extern _Bool sys_read_sys_long(const char *path,u_vlong *result);

//to read a value from system
extern _Bool sys_read_sys_long(const char *path,u_vlong *result);

//to write a value to system
extern _Bool sys_write_sys_long(const char *path,u_vlong tostore);

//move the process to another cgroup
extern _Bool sys_move_to_cgroup(const char *contname,CGRENU ext);

//to return MEMINFO contents as string like "used/max Kb"
extern const char *sys_meminf_printf(MEMINF *mems);

//to return BANDWITH accumulated (or speed) as string like " KB"
extern const char *sys_bandwidth_printf(BANTYP bandwidth[2],_Bool speed);

//to assign CPU to container
extern _Bool sys_setcpuset(const char *contname,double ratio);

//to get the current count of CPU assigned to container
extern uint16_t sys_get_numcpu(const char *contname);

//procedure to get the HOST CPU list
extern _Bool sys_get_cpu_host_list(PHYCPU *cpuslist);

//procedure to get the container current CPU assignment
extern _Bool sys_get_cpu_cont_list(PHYCPU *cpuslist);

//procedure to pin the namespace
extern _Bool sys_pin_cgroup_ns(pid_t cpid);

//OBSOLETE!!
//procedure to set the prepare the subtree_control
//extern _Bool sys_set_cont_subtree_control(const char *contname);

//procedure to set the limite CPU usage the hard-way
extern _Bool sys_set_cont_usage(const char *contname,double ratio);

//procedure to get the assigned container's CPU list
extern char *sys_get_cont_cpulist(const char *contname);

//procedure to set the weight value for the container
extern _Bool sys_set_cont_weight(const char *contname,double ratio);

//procedure to get the container/Host weight ration
extern char *sys_get_cont_weight(const char *contname);

//procedure to get the container cpu usage ceiling values
extern char *sys_get_cont_ceiling(const char *contname);

//procedure to get the container cpu usage values
extern _Bool sys_get_cpustat(CPUENU cpuenu,STATYP *status);

//procedure to return the cpu idle ration
extern double sys_get_cpu_idle(STATYP *status);

//procedure to write the container starting time within an start file
extern _Bool sys_set_start(pid_t contpid);

//procedure to read the container starting time 
extern _Bool sys_get_start(pid_t *contpid,u_vlong *start);

//procedure to return the host loadavg values
extern _Bool sys_get_host_loadavg(double *avgs,u_int taille);

//procedure to return a string about calculate the load average
extern _Bool sys_cal_loadavg(STATYP *status);

//procedure to create a new container status structure with inital values
extern STATYP *sys_new_cont_status();

//procedure to update critical status data
extern _Bool sys_update_cont_status(STATYP *status);

//procedure to free a container status structure with inital values
extern STATYP *sys_free_cont_status(STATYP *status);

//procedure to assign the random seed;
extern _Bool sys_set_random_seed();
#endif
