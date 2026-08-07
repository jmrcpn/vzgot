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
/*	Define utilities level procedure needed */
/*	by the application.			*/
/*						*/
/************************************************/
#ifndef UTLAPL
#define UTLAPL
#include	<unistd.h>
#include	<stdbool.h>
#include	<stdio.h>
#include	<sys/types.h>
#include	"lowtyp.h"

/*transit subdirectory name			*/
#define	TSUFFIX	"-S"

typedef	struct	{	/*vocable table type	*/
	int code;	/*code value		*/
	const char *key;/*key code		*/
	const void *ptr;/*pay load		*/
	}VOCTYP;

/*Signal flag definition			*/
extern 	int sigcont;
extern 	int sighup;
extern 	int sigint;
extern 	int sigquit;
extern 	int sigterm;

/*application current version number		*/
extern const char *curvers;

extern int apl_isdir(char *dirpath);
extern const char *apl_getvers();
extern u_long apl_getmillisec();
extern char *apl_uniquename(unsigned int seq);
extern u_long apl_date(time_t curtime);

//express an number of sec since current time from start
extern char *apl_uptime(char *uptime,int taille,u_vlong start);

//express an number of msec as time
extern char *apl_chrono_ms(char *chrono,int taille,u_vlong ms);

extern char *apl_ascsystime(time_t curtime);
extern char *apl_ascsysdate(time_t curtime);
extern char *apl_ascsysdatetime(time_t curtime);
extern char *apl_ascsysstamp(time_t curtime);
extern time_t apl_datetimesysasc(char *strdate,char *strtime);
extern void apl_argvtrace(const int dlevel,const char *fmt,char *argv[]);
extern void apl_trapsegv(int onoff);
extern void apl_core_dump(char *frmt, ...);
extern void apl_settrap(int set);
extern char *apl_strtolower(char *str);
extern char *apl_getstr(FILE *fichier,char *str,u_int taille,char carcom);
extern int apl_createdirs(char *dirname);
extern char *apl_getapvers();
extern VOCTYP *apl_getvoca(const VOCTYP *table,char *str);
extern char *apl_cleanstring(char *str);
extern unsigned int apl_dodelay(unsigned int delais);

//procedure to check and hanlde pending signal
extern int apl_checksig();

//procedure to convert a string to an (expected) double
extern double apl_getdouble(const char *valeur);

//procedure to find the application confing default directory
extern const char *apl_dfltconfdir();

#endif
