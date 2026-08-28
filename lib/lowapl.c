// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*	Implement low level procedure to 	*/
/*	manage application low level.           */
/*						*/
/************************************************/
#include        <malloc.h>
#include        <string.h>
#include        <stdio.h>

#include	"version.h"
#include	"lowapl.h"

/*application name				*/
PUBLIC	char	*appname=VZGOT;
/*

*/
/************************************************/
/*						*/
/*	Procedure to extract and return current	*/
/*	version number.				*/
/*						*/
/************************************************/
const char *apl_getvers()

{
return VERSION"."RELEASE;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to get application+version    */
/*      string.                                 */
/*						*/
/************************************************/
PUBLIC char *apl_getapvers()

{
static char *apvers=(char *)0;

if (apvers==(char *)0) {
  static char apinfo[50];

  char *ptr;
  char version[30];

  (void) strcpy(version,apl_getvers());
  if ((ptr=strchr(version,'-'))!=(char *)0)
    *ptr='\000';
  (void) snprintf(apinfo,sizeof(apinfo),"%s-%s/",appname,version);
  apvers=apinfo;
  }
return apvers;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to 'compute' an application 	*/
/*	directory according dir enum value	*/
/*						*/
/************************************************/
PUBLIC char *apl_appdir(DIRENUM dir)

{
char *appdir;
char *sysbase;
char *apvers;
char *subdir;

sysbase=(char *)0;
appdir=(char *)0;
apvers=apl_getapvers();
subdir="";
switch (dir) {
  case (d_null)		:
    sysbase="";
    apvers=""; 
    break;
  case (d_tmp)		:
    sysbase="/var/tmp";
    apvers=VZGOT"/"; 
    break;
  case (d_crash)	:
    sysbase="/var/crash";
    break;
  case (d_etc)		:
    sysbase="/etc";
    apvers=VZGOT"/"; 
    break;
  case (d_spool)	:
    sysbase="/var/spool";
    break;
  case (d_lock)		:
    sysbase="/run";
    apvers=VZGOT"/";
    break;
  case (d_vzgot)	:
    sysbase="/var/lib";
    apvers=VZGOT;
    subdir="/vzdir";	
    break;
  case (d_log)		:
    sysbase="/var/spool";
    subdir="logs";
    break;
  case (d_ubin)		:
    sysbase="/usr/bin";
    apvers=""; 
    break;
  case (d_usbin)	:
    sysbase="/usr/sbin";
    apvers=""; 
    break;
  case (d_varlib)	:
    sysbase="/var/lib";
    apvers=VZGOT; 
    break;
  case (d_usrlib)	:
    sysbase="/usr/lib";
    break;
  case (d_libexec)	:
    sysbase="/usr/libexec";
    apvers=VZGOT; 
    break;
  default		:
    /*something impossible !?		*/
    break;
  }
if (sysbase!=(char *)0) {
  int taille;

  taille=strlen(sysbase);
  taille+=strlen(apvers);
  taille+=strlen(subdir);
  taille+=4;	//spare space
  appdir=(char *)calloc(taille,sizeof(char)); 
  (void) snprintf(appdir,taille,"%s/%s%s",sysbase,apvers,subdir);
  }
return appdir;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to free memory used by a	*/
/*	string, do not proceed if point is NULL.*/
/*						*/
/************************************************/
PUBLIC char *apl_freestr(char *str)

{
if (str!=(char *)0) {
  (void) free(str);
  }
return (char *)0;
}
/*

*/
/************************************************/
/*						*/
/*	procedure to get the list of 'device'	*/
/*	needed to manage container system by	*/
/*	its supervisor.				*/
/*						*/
/************************************************/
PUBLIC const DEVTYP *apl_get_specdevs()

{
static const DEVTYP specdevs[]={
        {"cpuinfo",dev_cpuinfo},
        {"lastpid",dev_lastpid},
        {"loadavg",dev_loadavg},
        {"meminfo",dev_meminfo},
        {"swaps",dev_swaps},
        {"acpi",dev_acpi},
        {"bus",dev_bus},
        {"interrupts",dev_interrupts},
        {"ioports",dev_ioports},
        {"kcore",dev_kcore},
        {"mdstat",dev_mdstat},
        {"modules",dev_modules},
        {"partitions",dev_partitions},
        {"sysrq-trigger",dev_trigger},
        {(const char *)0,dev_unknown}
	};

return specdevs;
}

