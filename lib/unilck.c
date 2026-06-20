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
/*	UNILCK:					*/
/*	Take Care of all process locking	*/
/*	function.				*/
/*						*/
/************************************************/
#include	<sys/file.h>
#include	<sys/stat.h>
#include	<sys/types.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<limits.h>
#include	<string.h>
#include	<stdio.h>
#include	<syslog.h>
#include	<unistd.h>

#include	"lowtyp.h"
#include	"dbglog.h"
#include	"utlapl.h"
#include	"subprc.h"
#include	"unilck.h"

/*Application locking directory			*/
#define	LOCKEXT	"-lock"	/*lock file extension	*/

/*

*/
/************************************************/
/*						*/
/*	procedure to set/unset a lock 		*/
/*	return true if successful,		*/
/*	false otherwise.			*/
/*						*/
/************************************************/
int lck_locking(const char *ident,int lock,int tentative)

{
#define	OPEP	"unilck.c:lck_locking,"

int done;
int handle;	//lock file handle
char *dlock;
char lockname[PATH_MAX];
struct stat bufstat;
int phase;
int proceed;

done=false;
handle=0;
dlock=(char *)0;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0	:	//Finding lock directory name
      if ((dlock=apl_appdir(d_lock))==(char *)0) {
        (void) log_alert(0,"%s Unable to get lock directory position (Bug?)",
			    OPEP);
	phase=999;	//No need to go further
	}
      break;
    case 1	:	//creating the lock directory
      if (mkdir(dlock,0750)<0) {
	switch (errno) {
	  case EEXIST	:	//directory already existing
	    break;
	  default	:
            (void) log_alert(0,"%s Unable to create directory <%s>, "
			       "error=<%s> (config?)",OPEP,dlock,strerror(errno));
	    phase=999;		//No need to go further
	    break;
	  }
	}
      break;
    case 2	:	//making lockname
      (void) snprintf(lockname,sizeof(lockname),"%s%s%s",dlock,ident,LOCKEXT);
      if (lock==LCK_UNLOCK) {
	(void) log_alert(9,"%s Request unlocking <%s>",OPEP,lockname);
	(void) unlink(lockname);
	done=true;
	phase=999;	//No need to go further
	}
      break;
    case 3	:	//checking if link already exist
      if (stat(lockname,&bufstat)<0) {
	phase++;	//link doesn't exist
	}
      break;
    case 4	:	//making lockname
      if (S_ISREG(bufstat.st_mode)==0) {
        (void) log_alert(0,"<%s> is not a file!, unable to lock (sysadmin?)",
			   lockname);
	phase=999;	//major trouble
	break;		
	}
      while (tentative>0) {
        FILE *fichier;
	
        if ((fichier=fopen(lockname,"r"))==(FILE *)0)
	  break;	//lock file now missing, lets continue
	else {
          int pid;
          char strloc[80];

	  (void) memset(strloc,'\000',sizeof(strloc)); 
	  if (fgets(strloc,sizeof(strloc)-1,fichier)!=(char *)0) {
            if (sscanf(strloc,"%d",&pid)==1) {
              (void) log_alert(1,"Locking, check %d process active",pid);
	      if (prc_checkprocess(pid)==false) {
                (void) log_alert(1,"Locking, removing unactive lock");
	        (void) unlink(lockname);
                }
	      else {
                (void) log_alert(0,"found process '%d' to be "
			      	   "active (still trying to lock for %02d second)",
				   pid,tentative);
	        }
	      }
	    }
	  (void) fclose(fichier);
	  if (strlen(strloc)==0) {
            (void) log_alert(1,"Locking, removing empty lock");
	    (void) unlink(lockname);
	    }
	  (void) sleep(1);
	  }
	tentative--;
        if (tentative==0) {
          (void) log_alert(0,"Unable to lock container <%s> Giving up",
			      ident);
	  phase=999;	//trouble trouble
	  }
	}
      break;
    case 5	:	//making lockname
      (void) log_alert(9,"%s Request locking <%s>",OPEP,lockname);
      if ((handle=creat(lockname,0640))<0) {
        (void) log_alert(0,"Unable to create lock <%s> (error=<%s>) "
			    "Aborting!",lockname,strerror(errno));
	phase=999;	//trouble trouble
	}
      break;
    case 6	:	//getting an exclusive access
      if (flock(handle,LOCK_EX)<0) {
        (void) log_alert(0,"Unable to get exclusive acces "
			   "to lock <%s> (error=<%s>) "
			   "Aborting!",lockname,strerror(errno));
	(void) close(handle);
	phase=999;	//trouble trouble
	}
      break;
    case 7	:	//locking access to file
      if (handle>=0) {	//always
	int taille;
	char numid[30];

	(void) memset(numid,'\000',sizeof(numid));
        (void) snprintf(numid,sizeof(numid),"% 6d\n",getpid());
	taille=strlen(numid);
	if (write(handle,numid,taille)!=taille)
	  (void) log_alert(0,"container <%s> process pid '%06d' %s",
			     ident,getpid(),"(Unable to lock! bug?)");
	(void) flock(handle,LOCK_UN);
	(void) close(handle);
	(void) log_alert(1,"container <%s> process pid '%06d' now locked",
			   ident,getpid());
	done=true;
  	} 
      break;
    default	:	//SAFE Guard
      if (done==false) 
        (void) log_alert(0,"Unable to set <%s> lock (config?)",lockname);
      dlock=apl_freestr(dlock);
      proceed=false;
      break;
    }
  phase++;
  }
return done;
#undef	OPEP
}
