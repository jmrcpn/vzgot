// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*						*/
/*	Define utility level procedure to       */
/*      display container configuration and     */
/*      live status.                            */
/*						*/
/************************************************/
#ifndef UNICFG
#define UNICFG

#include	<stdbool.h>
#include	<stdio.h>

//ENV variable names 
#define	ONLINETPL	"ONLINETPL"	//the "online" template

//Reference to the online template
typedef	void *TPLPTR;

extern TPLPTR *cfg_open_online(const char *confdir,pid_t cont_pid,
                               const char *tplname);
extern _Bool cfg_update_online(const char *contname,TPLPTR *tplptr);
extern _Bool cfg_flush_online(FILE *dest,TPLPTR *tplptr);
extern TPLPTR *cfg_close_online(TPLPTR *tplptr);

#endif
