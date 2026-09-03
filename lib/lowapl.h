// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*	Define utility level procedure to handle*/
/*	CPU fonction				*/
/*						*/
/************************************************/
#ifndef LOWAPL
#define LOWAPL

#include	"lowapl.h"

/*vzgot application name			*/
#define	VZGOT		"vzgot"

/*defining application directory		*/
typedef	enum	{
	d_tmp,		/*directory /tmp	*/
	d_crash,	/*the crash directory	*/
	d_etc,		/*directory is /etc	*/
	d_ubin,		/*directory /usr/bin	*/
	d_usbin,	/*directory /usr/sbin	*/
	d_usrlib,	/*directory /usr/lib	*/
	d_libexec,	/*directory /usr/libexec*/
	d_varlib,	/*directory /var/lib	*/
	d_spool,	/*spool directory	*/
	d_log,		/*logs directory 	*/
	d_lock,		/*locking directory	*/
	d_vzgot,	/*application main dir	*/
	d_null		/*no directory specified*/
	}DIRENUM;


//All speciale devices list
typedef	enum	{
        dev_cpuinfo,    // container CPU data
        dev_lastpid,    // container last pid
        dev_loadavg,    // container load average
        dev_meminfo,    // container available memory
        dev_swaps,      // container available swap
        dev_acpi,       // /proc/acpi
        dev_bus,        // /proc/bus
        dev_interrupts, // /proc/interrupts
        dev_ioports,    // /proc/ioports
        dev_kcore,      // /proc/kcore
        dev_mdstat,     // /proc/mdstat
        dev_modules,    // /proc/modules
        dev_partitions, // /proc/partitions
        dev_trigger,    // /proc/sysrq-trigger
        dev_unknown     // sentinel.
        }DEVENUM;

//special devices structure
typedef struct  {
        const char *str;//device name
        DEVENUM devenum;//device enum
        }DEVTYP;

/*application name				*/
extern char *appname;

//procdure to get the list of special devices needed by container
extern const DEVTYP *apl_get_specdevs();

//to get the application version number
extern  char *apl_getapvers();

//Procedure to get a path according a dir name
extern char *apl_appdir(DIRENUM dir);

//procedure to free dynamically allocated string
extern char *apl_freestr(char *str);

#endif
