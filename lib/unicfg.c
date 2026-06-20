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
/*	Implement utility level procedure to	*/
/*	local application specific configuration*/
/*	All collected variable are stored	*/
/*	within environement variables		*/
/*						*/
/************************************************/
#include	<errno.h>
#include	<limits.h>
#include	<memory.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	"dbglog.h"
#include	"lowtyp.h"
#include	"utlapl.h"
#include	"utlsys.h"
#include	"utlvec.h"
#include	"subcfg.h"
#include	"subprc.h"
#include	"unicfg.h"


typedef	enum	{
	tpl_ceiling,	// Container/Host CPUs ceiling ratio
	tpl_chwratio,	// Container/Host CPUs weight ratio
	tpl_coomkill,	// Container Out of memory event detected
	tpl_cint,	// Container interface
	tpl_cip,	// Container IP
	tpl_cmem,	// Container memory ratio
	tpl_cmpress,	// Container memory pressure
	tpl_cname,	// Container name
	tpl_ctrxtx,	// Container total bandwidth
	tpl_csrxtx,	// Host bandwidth speed
	tpl_cswap,	// Container swap ratio
	tpl_cpuidle,	// Container idle time ratio
	tpl_cpuused,	// Container CPUs accumulated working time
	tpl_cpuwait,	// Container CPUs accumulated waiting mode
	tpl_hmem,	// Host Interface
	tpl_hmpress,	// Host memroy pressure
	tpl_hint,	// Host IP
	tpl_hip,	// Host memory ratio
	tpl_hmodel,	// Host cpu type model name
	tpl_hname,	// Host name
	tpl_hoomkill,	// Host Out of memory event detected
	tpl_hsrxtx,	// Host bandwidth speed
	tpl_htrxtx,	// Host total bandwidth
	tpl_hswap,	// Host swap ratio
	tpl_lavg,	// Container load average
	tpl_lstass,	// List of cpu assigned to container
	tpl_model,	// Host Cpu model
	tpl_maxcpu,	// Container config variable MAXCPU
	tpl_memmax,	// Container config variable MEMMAX
	tpl_numcpu,	// Container config variable NUMCPU
	tpl_onl,	// HOST cpu online
	tpl_pid,	// Container PID
	tpl_pidmax,	// Container config variable PIDMAX
	tpl_pwrcpu,	// Container config variable PWRCPU
	tpl_swapmax,	// Container config variable SWAPMAX
	tpl_throttle,	// Container number of throttle events
	tpl_uptime,	// Container uptime
	tpl_users,	// Number of container's user
	tpl_unknown	// Unknown variable
	}TPLENUM;

typedef	struct	{
	TPLENUM code;	//template code
	char *format;	//display format;
	}VARTYP;

typedef	struct	{	//template information
	VARTYP **vars;	//variable list	
	char **lines;	//lines templates
	char **updated;	//line to be displayed
	STATYP *status;	//Container status data
	}TPLTYP;

typedef struct {
	const char *varname;	//known variable
	TPLENUM code;		//template code
	}ID_TYP;

static ID_TYP voc[]={
	{"CEILING",tpl_ceiling},
	{"COOMKILL",tpl_coomkill},
	{"CHWRATIO",tpl_chwratio},
	{"CINT",tpl_cint},
	{"CIP",tpl_cip},
	{"CMEM",tpl_cmem},
	{"CMPRESS",tpl_cmpress},
	{"CNAME",tpl_cname},
	{"CPUIDLE",tpl_cpuidle},
	{"CPUUSED",tpl_cpuused},
	{"CPUWAIT",tpl_cpuwait},
	{"CTRXTX",tpl_ctrxtx},
	{"CSRXTX",tpl_csrxtx},
	{"CSWAP",tpl_cswap},
	{"HINT",tpl_hint},
	{"HIP",tpl_hip},
	{"HMEM",tpl_hmem},
	{"HNAME",tpl_hname},
	{"HMODEL",tpl_hmodel},
	{"HMPRESS",tpl_hmpress},
	{"HOOMKILL",tpl_hoomkill},
	{"HSRXTX",tpl_hsrxtx},
	{"HSWAP",tpl_hswap},
	{"HTRXTX",tpl_htrxtx},
	{"LOADAVG",tpl_lavg},
	{"LSTASS",tpl_lstass},
	{MEMMAX,tpl_memmax},
	{MAXCPU,tpl_maxcpu},
	{NUMCPU,tpl_numcpu},
	{"ONL",tpl_onl},
	{"PID",tpl_pid},
	{PIDMAX,tpl_pidmax},
	{PWRCPU,tpl_pwrcpu},
	{SWAPMAX,tpl_swapmax},
	{"THROTTLE",tpl_throttle},
	{"USERS",tpl_users},
	{"UPTIME",tpl_uptime},
	{(const char *)0,tpl_unknown}
	};
	
/*

*/
/************************************************/
/*						*/
/*	Procedure to fee a VARTP structure	*/
/*	the env area.				*/
/*						*/
/************************************************/
static VARTYP *freevar(VARTYP *var)

{
if (var!=(VARTYP *)0) {
  var->format=apl_freestr(var->format);
  (void) free(var);
  var=(VARTYP *)0;
  }
return var;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to add a variable as $CONTNAME*/
/*	To the list of known variables.		*/
/*						*/
/************************************************/
static const ID_TYP *lookup(char *varname)

{
const ID_TYP *found;
size_t taille;

found=(const ID_TYP *)0;
taille=strlen(varname);
for (int i=0;voc[i].varname!=(const char *)0;i++) {
  if (strncmp(varname,voc[i].varname,taille)==0) {
    found=&(voc[i]);
    break;
    }
  }
return found;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to add a variable as $CONTNAME*/
/*	To the list of known variables.		*/
/*						*/
/************************************************/
static _Bool scanvar(TPLTYP *tpl,char *line,unsigned int numline)

{
#define OPEP	"unicfg.c:scanvar"
#define	TAILLE	49


_Bool isok;
char name[TAILLE+1];
char format[TAILLE+1];
const ID_TYP *found;
VARTYP *var;
int phase;
_Bool proceed;

isok=false;
(void) memset(name,'\000',sizeof(name));
(void) memset(format,'\000',sizeof(format));
found=(const ID_TYP *)0;
var=(VARTYP *)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Parse variable and format
      if (sscanf(line,"%49[^=]=<%49[^>]>",name,format)!=2) {
        (void) log_alert(0,"%s Unable to parse <%s> (config?)",OPEP,line);
	phase=999;	//no need to go further
	}
      break;
    case 1	:	//Check for duplicate variables in template
      
      if ((found=lookup(name))==(const ID_TYP *)0) {
        (void) log_alert(0,"%s Unknown variable <%s> from line <%03u:%s> (config?)",
			    OPEP,name,numline,line);
	phase=999;	//no need to go further
	}
      for (int i=0;voc[i].varname!=(const char *)0;i++) {
	if (strcmp(name,voc[i].varname)==0) {
	  break;
	  }
	}
      break;
    case 2	:	//search if we have already the variable
      if (tpl->vars!=(VARTYP **)0) {
	VARTYP **ptr;

	ptr=tpl->vars;
	while (*ptr!=(VARTYP *)0) {
	  if (found->code==(*ptr)->code) {
            (void) log_alert(0,"%s var <%s> %s; see line %03u:\"%s\" (config?)",
				OPEP,name,"already declared",numline,line);
	    phase=999;	//no need to go further
	    break;
	    }
	  ptr++;
	  }
	}
      break;
    case 3	:	//everything fine variable extracted is new
      var=(VARTYP *)calloc(1,sizeof(VARTYP));
      var->code=found->code;
      var->format=strdup(format);
      tpl->vars=(VARTYP **)vec_addlstlst((void **)tpl->vars,(void *)var);
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	TAILLE
#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to resolve variable values	*/
/*						*/
/************************************************/
static _Bool do_resolv(TPLTYP *tpl,size_t using,VARTYP *var,char *marker,size_t remain)

{
#define	OPEP	"unicfg.c:do_resolv"

static const char *mapping[] = {
	[tpl_maxcpu]  = MAXCPU,
	[tpl_memmax]  = MEMMAX,
	[tpl_numcpu]  = NUMCPU,
	[tpl_pidmax]  = PIDMAX,
	[tpl_pwrcpu]  = PWRCPU,
	[tpl_swapmax] = SWAPMAX
	};

_Bool isok;
int num;
const char *data;
STATYP *status;
CPUINF *cpudata;
char line[100];
char uptime[40];
char replace[60];

isok=true;
data=(const char *)0;
status=tpl->status;
cpudata=&(status->cpuinf[cpu_cont]);
num=0;
switch (var->code) {
  case tpl_ceiling	:
    num=snprintf(replace,sizeof(replace),var->format,status->ceiling);
    break;
  case tpl_chwratio	:
    num=snprintf(replace,sizeof(replace),var->format,status->weight);
    break;
  case tpl_cint	:
    num=snprintf(replace,sizeof(replace),var->format,status->network[cpu_cont].intname);
    break;
  case tpl_cip	:
    num=snprintf(replace,sizeof(replace),var->format,status->network[cpu_cont].ip_num);
    break;
  case tpl_cmpress	:
    num=snprintf(replace,sizeof(replace),var->format,status->oom[cpu_cont].avg10);
    break;
  case tpl_coomkill	:
    num=snprintf(replace,sizeof(replace),var->format,status->oom[cpu_cont].oom_kill);
    break;
  case tpl_cpuidle	:
    num=snprintf(replace,sizeof(replace),var->format,sys_get_cpu_idle(status));
    break;
  case tpl_cpuused	:
    if (apl_chrono_ms(uptime,sizeof(uptime),cpudata->usage/1000)!=(char *)0) {
      num=snprintf(replace,sizeof(replace),var->format,uptime);
      }
    break;
  case tpl_cpuwait	:
    if (apl_chrono_ms(uptime,sizeof(uptime),cpudata->throttled/1000)!=(char *)0) {
      num=snprintf(replace,sizeof(replace),var->format,uptime);
      }
    break;
  case tpl_hint	:
    num=snprintf(replace,sizeof(replace),var->format,status->network[cpu_host].intname);
    break;
  case tpl_hip	:
    num=snprintf(replace,sizeof(replace),var->format,status->network[cpu_host].ip_num);
    break;
  case tpl_lavg		:
    num=snprintf(replace,sizeof(replace),var->format,status->loadavg);
    break;
  case tpl_lstass	:
    num=snprintf(replace,sizeof(replace),var->format,status->cpuassigned);
    break;
  case tpl_cname		:
    num=snprintf(replace,sizeof(replace),var->format,status->contname);
    break;
  case tpl_onl		:
    num=snprintf(replace,sizeof(replace),var->format,status->cpuonl);
    break;
  case tpl_hoomkill	:
    num=snprintf(replace,sizeof(replace),var->format,status->oom[cpu_host].oom_kill);
    break;
  case tpl_pid		:
    num=snprintf(replace,sizeof(replace),var->format,status->contpid);
    break;
  case tpl_cmem		:
    data=sys_meminf_printf(&(status->mems[cpu_cont][mem_mem]));
    num=snprintf(replace,sizeof(replace),var->format,data);
    break;
  case tpl_csrxtx	:
    data=sys_bandwidth_printf(status->network[cpu_cont].traffic,true);
    num=snprintf(replace,sizeof(replace),var->format,data);
    break;
  case tpl_ctrxtx	:
    data=sys_bandwidth_printf(status->network[cpu_cont].traffic,false);
    num=snprintf(replace,sizeof(replace),var->format,data);
    break;
  case tpl_cswap	:
    data=sys_meminf_printf(&(status->mems[cpu_cont][mem_swap]));
    num=snprintf(replace,sizeof(replace),var->format,data);
    break;
  case tpl_hmem		:
    data=sys_meminf_printf(&(status->mems[cpu_host][mem_mem]));
    num=snprintf(replace,sizeof(replace),var->format,data);
    break;
  case tpl_hmpress	:
    num=snprintf(replace,sizeof(replace),var->format,status->oom[cpu_host].avg10);
    break;
  case tpl_hname	:
    (void) gethostname(line,sizeof(line));
    num=snprintf(replace,sizeof(replace),var->format,line);
    break;
  case tpl_hmodel		:
    num=snprintf(replace,sizeof(replace),var->format,status->cpumodel);
    break;
  case tpl_hsrxtx	:
    data=sys_bandwidth_printf(status->network[cpu_host].traffic,true);
    num=snprintf(replace,sizeof(replace),var->format,data);
    break;
  case tpl_htrxtx	:
    data=sys_bandwidth_printf(status->network[cpu_host].traffic,false);
    num=snprintf(replace,sizeof(replace),var->format,data);
    break;
  case tpl_hswap	:
    data=sys_meminf_printf(&(status->mems[cpu_host][mem_swap]));
    num=snprintf(replace,sizeof(replace),var->format,data);
    break;
  case tpl_maxcpu	:
  case tpl_memmax	:
  case tpl_numcpu	:
  case tpl_pidmax	:
  case tpl_pwrcpu	:
  case tpl_swapmax	:
    if ((data=getenv(mapping[var->code]))!=(char *)0) {
      double valeur;
	
      valeur=apl_getdouble(data);
      num=snprintf(replace,sizeof(replace),var->format,valeur);
      }
    break;
  case tpl_throttle	:
    num=snprintf(replace,sizeof(replace),var->format,cpudata->nr_throttled);
    break;
  case tpl_uptime	:
    if (apl_uptime(uptime,sizeof(uptime),status->start)!=(char *)0) {
      num=snprintf(replace,sizeof(replace),var->format,uptime);
      }
    break;
  case tpl_users	:
    num=snprintf(replace,sizeof(replace),var->format,status->nbruser);
    break;
  default		:	//code missing!
    (void) log_alert(0,"%s Unexpected code='%d' (%s)",
		      OPEP,var->code,"Code not yet implemented!");
    isok=false;
    break;
  }
if (num>0) {
  int max;

  if (using>num) 	//to remove en of Key, if key is longer than result
    (void) memmove(marker+num,marker+using,strlen(marker+using)+1);
  max=strlen(marker); 
  if (num>remain)
    num=remain;
  (void) memcpy(marker,replace,num);
  if (num>=max)
    marker[num]='\000'; 	//adding and end of line
  }
return isok;

#undef OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to adjust line contents when	*/
/*	a key is detecteds.			*/
/*						*/
/************************************************/
static _Bool adjusting(TPLTYP *tpl,char *marker,size_t remain)

{
#define	OPEP	"unicfg.c:adjusting"

_Bool isok;
const ID_TYP *found;
VARTYP *var;
char var_name[50];
int phase;
_Bool proceed;

isok=false;
found=(const ID_TYP *)0;
var=(VARTYP *)0;
(void) memset(var_name,'\000',sizeof(var_name));
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d'",OPEP,phase);
  switch (phase) {
    case 0	:	//extracting the key
      if (sscanf(marker,"$%49[^:]",var_name)!=1) {
        (void) log_alert(0,"%s Unable find variable in line <%s> (Bug!)",
			    OPEP,marker);
        break;		//Trouble trouble
        }
      break;
    case 1	:	//To have XXXX to replace unknown variable
			//in converted text
      if ((found=lookup(var_name))==(const ID_TYP *)0) {
        (void) log_alert(0,"%s JMPDBG marker=<%s>",OPEP,marker);
        (void) log_alert(0,"%s variable <%s> is unknown (Online template?)",
			    OPEP,var_name);
	// Replace unknown variable with 'X' placeholders in converted text
	(void)memset(marker,'X',strlen(var_name));
	phase=999;	//no need to go further
	}
      break;
    case 2	:	//Looking for the defined variable
      if (tpl->vars!=(VARTYP **)0) {
	VARTYP **ptr;

	ptr=tpl->vars;
	while (*ptr!=(VARTYP *)0) {
    	  if (found->code==(*ptr)->code) {
            var=(*ptr);
            break;
            }
	  ptr++;
	  }
        }
      if (var==(VARTYP *)0) {
        (void) log_alert(0,"%s variable <%s> not defined (Online Template?)",
			    OPEP,var_name);
	// Replace unknown variable with 'Y' placeholders in converted text
	(void)memset(marker,'Y',strlen(var_name));
	phase=999;	//Undefined variable fallback
	}
      break;
    case 3	:
      if ((isok=do_resolv(tpl,strlen(var_name)+2,var,marker,remain))==false) {
        (void) log_alert(0,"%s Unable to resolve var_name=<%s> (template?)",
			    OPEP,var_name);
	(void)memset(marker,'Z',strlen(var_name));
	phase=999;	//note converted variable fallback
	}
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to create a template structure*/
/*	to be used to display container online	*/
/*	data.					*/
/*						*/
/************************************************/
PUBLIC TPLPTR *cfg_open_online(const char *confdir,const char *contname,const char *tplname)

{
#define	OPEP	"unicfg.c:cfg_open_online"

TPLTYP *tpl;
FILE *fichier;
unsigned int numline;
char ppath[PATH_MAX];
char line[300];
_Bool isok;
int phase;
_Bool proceed;

tpl=(TPLTYP *)0;
fichier=(FILE *)0;
numline=0;
(void)memset(ppath,'\000',sizeof(ppath));
(void)memset(line,'\000',sizeof(line));
isok=true;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//checking if we have everything
      if (confdir==(char *)0) 
	confdir=apl_dfltconfdir();
      if ((confdir==(char *)0)||(tplname==(char *)0)) {
	(void) log_alert(0,"%s, confdir=<%s> or tplname=<%s> missing (config?)",
			    OPEP,confdir,tplname);
	phase=999;	//trouble trouble
	}
      (void) snprintf(ppath,sizeof(ppath),"%s/%s",confdir,tplname);
      break;
    case 1	:	//opening the file
      if ((fichier=fopen(ppath,"r"))==(FILE *)0) {
	(void) log_alert(0,"%s, Unable to open file <%s> (error=<%s> config?)",
			    OPEP,ppath,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 2	:	//opening the file
      tpl=(TPLTYP *)calloc(1,sizeof(TPLTYP));
      tpl->status=sys_new_cont_status();
      break;
    case 3	:	//scanning template
      while ((fgets(line,sizeof(line)-1,fichier))!=(char *)0) {
	numline++;
	(void) apl_cleanstring(line);
	switch(line[0]) {
	  case	0	:	//no line
	  case '\n'	:	//empty line
	  case '#'	:	//comment
	    break	;	//nothing to do
	  case	'$'	:	//variable
	    isok&=scanvar(tpl,line+1,numline);
	    break	;
	  case	'>'	:	//template line
	    tpl->lines=(char **)vec_addlstlst((void **)(tpl->lines),
					      (void *)strdup(line+1));
	    break	;
	  default	:	//unexpected line type
	    (void) log_alert(0,"%s, expected template line <%s> (config>)",
			    OPEP,line);
	    break;
	  }
	}
      (void) fclose(fichier);
      break;
    case 4	:	//is trouble detected freeing all TPL memoyr
      if (isok==false)
	tpl=(TPLTYP *)cfg_close_online((TPLPTR *)tpl);	//trouble detected
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return (TPLPTR *)tpl;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to update the online structure*/
/*	with live ontainer data.		*/
/*						*/
/************************************************/
PUBLIC _Bool cfg_update_online(const char *contname,TPLPTR *tplptr)

{
#define	OPEP	"unicfg.c:cfg_update_online"

_Bool isok;
TPLTYP *tpl;
char **lines;
int phase;
_Bool proceed;

isok=false;
tpl=(TPLTYP *)tplptr;
lines=(char **)0;
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s JMPDBG phase='%d'",OPEP,phase);
  switch (phase) {
    case 0	:	//Check if the template structure is ready
      if ((tpl==(TPLTYP *)0)||(tpl->lines==(char **)0)) {
	(void) log_alert(0,"%s, Template structure is not yet defined (Bug?)",OPEP);
	phase=999;	//No need to go further
	}
      break;
    case 1	:	//updating status
      if (sys_update_cont_status(tpl->status)==false) {
	(void) log_alert(0,"%s, Unable to get container <%s> status (system?)",
			    OPEP,contname);
	phase=999;	//No need to go further
	}
      break;
    case 2	:	//Remove previous conversion data if any remains
      tpl->updated=(char **)vec_freelstlst((void **)(tpl->updated),VECFREE);
      break;
    case 3	:	// Perform line-by-line conversion
      lines=tpl->lines;
      while (*lines!=(char *)0) {
	char work[400];
	char *scan;
	int numvar;

	numvar=10;
	(void) snprintf(work,sizeof(work),"%s",*lines);
	scan=work;
	while (*scan!='\000') {
	  size_t cpt;
	  size_t remain;
	  char *ptr;

	  cpt=strcspn(scan,"$");
	  if (scan[cpt]=='\000') 
	    break;		//Abort scan: no more '$' characters found
	  ptr=scan+cpt;		//ptr is positioned on the '$' character
	  remain=sizeof(work)-(size_t)(ptr-work);
	  if (adjusting(tpl,ptr,remain)==false)
	    break;	//Unable to fully adjust current line
	  scan=ptr;	//search for the next '$'
			//Note: can be recursive if the resolved value
			// 	contains a '$'
	  numvar--;
	  if (numvar==0) {
	    (void) log_alert(0,"%s, too many variable on line <%s> (template?)",
				OPEP,*lines);
	    break;
	    }
	  }
	tpl->updated=(char **)vec_addlstlst((void **)tpl->updated,strdup(work));
	lines++;
	}
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to store current online values*/
/*	to a file.				*/
/*						*/
/************************************************/
PUBLIC _Bool cfg_flush_online(FILE *dest,TPLPTR *tplptr)

{
#define	OPEP	"unicfg.c:cfg_flush_online"

_Bool isok;
TPLTYP *tpl;
char **toshow;
int phase;
_Bool proceed;

isok=false;
tpl=(TPLTYP *)tplptr;
toshow=(char **)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Checking template ready
      if ((tpl==(TPLTYP *)0)||(tpl->updated==(char **)0)) {
	(void) log_alert(0,"%s, Template structure is not yet ready (Bug?)",OPEP);
	phase=999;
	}
      break;
    case 1	:	//displaying information
      toshow=tpl->updated;
      while (*toshow!=(char *)0) {
	(void) fprintf(dest,"%s\n",*toshow);
	toshow++;
	}
      break;
    case 2	:	//free the converted data
      tpl->updated=(char **)vec_freelstlst((void **)(tpl->updated),VECFREE);
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to free all memory used to	*/
/*	generate template file.			*/
/*						*/
/************************************************/
PUBLIC TPLPTR *cfg_close_online(TPLPTR *tplptr)

{
#define	VARFREE	(void *(*)(void *))freevar

TPLTYP *tpl;

tpl=(TPLTYP *)tplptr;
if (tpl!=(TPLTYP *)0) {
  tpl->status=sys_free_cont_status(tpl->status);
  tpl->updated=(char **)vec_freelstlst((void **)(tpl->updated),VECFREE);
  tpl->lines=(char **)vec_freelstlst((void **)(tpl->lines),VECFREE);
  tpl->vars=(VARTYP **)vec_freelstlst((void **)(tpl->vars),VARFREE);
  (void) free(tpl);
  tpl=(TPLTYP *)0;
  }
return (TPLPTR *)tpl;

#undef VARFREE
}
