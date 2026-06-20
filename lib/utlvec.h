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
/*	Define utility level procedure to manage*/
/*	dynamic memory list			*/
/*						*/
/************************************************/
#ifndef UTLVEC
#define UTLVEC
#include	<stdlib.h>

#define	VECFREE	(void *(*)(void *))free


/*Vector list definition 			*/
/*Doubly-circularly-linked lists		*/
typedef struct LIST VECLST;
typedef struct LIST {
	void *payload;	/*payload		*/
	VECLST *prv;	/*previous node		*/
	VECLST *nxt;	/*next node		*/
	}VECTOR;

typedef	struct		{
	u_int numvec;	/*number of vector	*/
	VECLST *vcl;	/*vector list		*/
	}VPTR;

extern VPTR *vec_addveclst(VPTR *vptr,void *payload);
extern int vec_rmveclst(VPTR **vptr,void *payload);
extern VPTR *vec_freeveclst(VPTR *vptr,void *(*freepayload)(void *));
extern unsigned int vec_sizelstlst(void **lptr);
extern void **vec_addlstlst(void **lptr,void *payload);
extern void **vec_mrglstlst(void **lptr,void **ltoadd);
extern int vec_rmlstlst(void **lptr,void *payload);
extern void **vec_freelstlst(void **lptr,void *(*freepayload)(void *));
#endif
