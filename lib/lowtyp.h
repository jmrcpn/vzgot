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
/*	Define very bottom level structure type	*/
/*						*/
/************************************************/
#ifndef LOWTYP
#define LOWTYP

/*defining troolean value			*/
#include	<stdbool.h>


/*vzgot application name			*/
#define	VZGOT		"vzgot"

/*	environement KEY			*/
//ENV SYSF value
#define	VZCGROUP	"VZCGROUP"

//ENV working container home directory
#define	HOMEFS		"HOMEFS"

//ENV container name
#define	NAME		"NAME"

//ENV host bridge interface
#define	BRIDGENAME	"BRIDGENAME"
//ENV host container interface
#define	ETHNAME		"ETHNAME"

//macro definition
//to extrat winthin a struct variable size
#define field_sizeof(type,field) sizeof(((type *)0)->field)

typedef struct timespec TIMETIC;

/*a 8 Bytes wide integer			*/
typedef unsigned long long u_vlong;
typedef 	 long long vlong;

#define	CHARNULL (void *)0

//Project min/max Macro
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#endif
