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
#include	<sys/time.h>
#include	<errno.h>
#include	<limits.h>
#include	<memory.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	"dbglog.h"
#include	"lowtyp.h"
#include	"utlapl.h"
#include	"utlprc.h"
#include	"subcfg.h"

//vzgot configuration file
#define	VZCONF	"vzgot_config"

//directory within configuration directory 
//with container configuration
#define	NAMES	"names"

//name of the container infos file
#define	CONTINF	"cont_info"

/*

*/
/************************************************/
/*						*/
/*	Procedure to add a file contents to	*/
/*	the env area.				*/
/*						*/
/************************************************/
static int gesenv(char *filename,int toadd)

{
#define	OPEP	"utlcfg.c:gesenv,"

int status;
FILE *fichier;

status=1;
if ((fichier=fopen(filename,"r"))!=(FILE *)0) {
  u_int numline;
  char line[1000];

  status=0;
  numline=0;
  while (apl_getstr(fichier,line,sizeof(line),'#')!=(char *)0) {
    char var[101];
    char contents[201];

    numline++;
    (void) memset(var,'\000',sizeof(var));
    (void) memset(contents,'\000',sizeof(contents));
    if (strlen(line)==0)
      continue;		//empty line
    if (strchr(line,'=')==(char *)0)
      continue;		//not en environement 
    if (sscanf(line,"%100[^=]=%199s",var,contents)!=2) {
      (void) log_alert(0,"%s filename:<%s> line %04d:<%s> invalid format",
			  OPEP,filename,numline,line);
      continue;
      }
    if (contents[0]=='"') {
      char *ptr;

      (void) memmove(contents,contents+1,strlen(contents+1)+1);
      if ((ptr=strchr(contents,'"'))!=(char *)0)
        *ptr='\000';
      } 
    if (toadd==true) {
      if (setenv(var,contents,true)<0) 
        (void) log_alert(0,"%s unable to add '%s,%s' to env (error=<%s>)",
			    OPEP,var,contents,strerror(errno));
      }
    else {
      if (unsetenv(var)<0) 
        (void) log_alert(0,"%s unable to remove '%s' from env (error=<%s>)",
			    OPEP,var,strerror(errno));
      }
    } 
  (void) fclose(fichier);
  }
else
  (void) log_alert(0,"%s unable to open file '%s' (error=<%s>)",
		      OPEP,filename,strerror(errno));
return status;

#undef OPEP
}
/*

*/
/************************************************/
/*						*/
/*	loading local variable related to one	*/
/*	container(VPS)				*/
/*	Return 0 if configuation is loaded	*/
/*	1 otherwise.				*/
/*						*/
/************************************************/
PUBLIC int cfg_loadconfig(const char *confdir,const char *contname)

{
#define	OPEP	"utlcfg.c:cfg_loadconfig"

int status;
char conffile[PATH_MAX];
int phase;
int proceed;

status=0;
if (confdir==(const char *)0)
  confdir=apl_dfltconfdir();
phase=0;
proceed=true;
while (proceed==true) {
  //(void) log_alert(0,"%s, Phase='%d', confdir=<%s>",OPEP,phase,confdir);
  switch (phase) {
    case 0	:	/*Loading main config	*/
      (void) snprintf(conffile,sizeof(conffile),"%s/%s",confdir,VZCONF);
      status=gesenv(conffile,true);
      break;
    case 1	:	/*do we have contname	*/
      if (contname==(char *)0) 
	proceed=false;
      break;
    case 2	:	/*now VPS definitions	*/
      (void) snprintf(conffile,sizeof(conffile),"%s/%s/%s",confdir,NAMES,contname);
      status=gesenv(conffile,true);
      break;
    default	:	/*SAFE Guard		*/
      proceed=false;
      break;
    }
  phase++;
  if (status>0)
    break;
  }
return status;

#undef	OPEP
}
/*

*/
/************************************************/
/*						*/
/*	procedure to free memory used to store	*/
/*	container information.			*/
/*						*/
/************************************************/
PUBLIC INFTYP *cfg_freeinfos(INFTYP *infos)

{
if (infos!=(INFTYP *)0) {
  (void) free(infos);
  infos=(INFTYP *)0;
  }
return infos;
}
/*

*/
/************************************************/
/*						*/
/*	procedure to assign info structure	*/
/*	memory.					*/
/*						*/
/************************************************/
PUBLIC INFTYP *cfg_init_infos(const char *contname,pid_t cpid)

{
INFTYP *infos;
struct timeval tv;
u_vlong start;		//expressed in usec

infos=(INFTYP *)calloc(1,sizeof(INFTYP));
(void) gettimeofday(&tv,(struct timezone *)0);
start=(tv.tv_sec*1000000ULL)+tv.tv_usec;
(void) snprintf(infos->name,sizeof(infos->name),"%s",contname);
infos->start=start;
infos->pid=cpid;
return infos;
}
