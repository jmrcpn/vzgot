// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*	Implement procedure to interface the	*/
/*	application with apparmor		*/
/*	handle clement specific needs.		*/
/*						*/
/************************************************/
#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
#include        <errno.h>

#include	"dbglog.h"
#include	"utlapp.h"

#ifdef  HAVE_APPARMOR
#include	<sys/apparmor.h>
#else
#define aa_change_onexec(x)     1
#endif

/*

*/
/************************************************/
/*						*/
/*	Procedure to load the apparmor profile	*/
/*						*/
/************************************************/
PUBLIC _Bool app_load_profile(const char *profile)

{
#define OPEP    "utlapp.c:app_load_profile"

_Bool isok;
int phase;
_Bool proceed;

isok=true;
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //Checking if we have apparmor profile
      if (profile==(const char *)0) {
        isok=true;      //No profile to load, this is OK
        phase=999;
        }
      break;
    case 1      :       //loading profile
      isok=true;
      switch (aa_change_onexec(profile)) {
        case -1 :
          isok=false;
          (void) log_alert(0,"%s Unable to do aa_change_onexec <%s> (error=<%s>)",
			      OPEP,profile,strerror(errno));
	  phase=999;	/*trouble trouble	*/
          break;
        case  0 :
          (void) log_alert(0,"%s Profile <%s> under apparmor",OPEP,profile);
          break;
        default :       //we are NOT In apparmore mode
          break;
        }
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef  OPEP
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to parse a apparmor profile   */
/*      to be replaced/removed in/from kernel.  */
/*						*/
/************************************************/
_Bool app_parse_profile(const char *profile,_Bool replace)

{
#define OPEP    "utlapp.c:app_parse_profile"

_Bool isok;
int status;
char opt;
char cmd[200];
int phase;
_Bool proceed;

isok=false;
status=0;
opt='R';
cmd[0]='\000';
phase=0;
proceed=true;
while (proceed==true) {
  switch (phase) {
    case 0      :       //Do we have a profile defined
      if (profile==(const char *)0) {
        isok=true;      //No profile to load, this is OK
        phase=999;
        }
      break;
    case 1      :       //the commande
      if (replace==true)
        opt='r';
      (void) snprintf(cmd,sizeof(cmd),"apparmor_parser -%c /etc/apparmor.d/%s",
                                       opt,profile);
      if ((status=system(cmd))<0) {
        (void) log_alert(0,"%s Unable to execute commande <%s> (%s?)",
			   OPEP,cmd,"Apparmor package missing?");
	phase=999;	/*trouble trouble	*/
        } 
      break;
    case 2      :       //reporting command detail
      if (status!=0) {
        (void) log_alert(0,"%s commande <%s> status='%d' (trouble?)",
			   OPEP,cmd,status);
	phase=999;	/*trouble trouble	*/
        } 
      break;
    case 3      :       //everything fine
      isok=true;
      break;
    default	:	//SAFE Guard
      proceed=false;
      break;
    }
  phase++;
  }
return isok;

#undef OPEP
}


