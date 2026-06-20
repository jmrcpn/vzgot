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
/*	Implement utlity level procedure to 	*/
/*	manage linked list			*/
/*						*/
/************************************************/
#include	<stdio.h>
#include	<memory.h>
#include	"lowtyp.h"
#include	"utlvec.h"
/*

*/
/************************************************/
/*						*/
/*	Procedure to add a payload within	*/
/*	a vectored list				*/
/*						*/
/************************************************/
VPTR *vec_addveclst(VPTR *vptr,void *payload)

{
VECLST *node;

node=(VECLST *)calloc(1,sizeof(VECLST));
node->payload=payload;
if (vptr==(VPTR *)0) {
  vptr=(VPTR *)calloc(1,sizeof(VPTR));
  vptr->vcl=node;
  node->nxt=node;
  node->prv=node;
  }
else {
  node->nxt=vptr->vcl;
  node->prv=vptr->vcl->prv;
  vptr->vcl->prv->nxt=node;
  vptr->vcl->prv=node;
  }
vptr->numvec++;
return vptr;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to remove on vector from the	*/
/*	vector list, return the number of	*/
/*	payload subtracted.			*/
/*						*/
/************************************************/
int vec_rmveclst(VPTR **vptr,void *payload)

{
register int done;

done=0;
if (*vptr!=(VPTR *)0) {
  register u_int i;
  register VECLST *cur;

  for (i=0,cur=(*vptr)->vcl;i<(*vptr)->numvec;i++) {
    if (payload==cur->payload) {
      register VECLST *prv;

      prv=cur->prv;
      (*vptr)->numvec--;
      cur->prv->nxt=cur->nxt;
      cur->nxt->prv=cur->prv;
      (void) free(cur);
      cur=prv;
      done++;
      }
    cur=cur->nxt;
    }
  if ((*vptr)->numvec==0) {
    (void) free(*vptr);
    *vptr=(VPTR *)0;
    }
  }
return done;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to free a vector list, 	*/
/*	memory used by payload is freed too.	*/
/*						*/
/************************************************/
VPTR *vec_freeveclst(VPTR *vptr,void *(*freepayload)(void *))

{
if (vptr!=(VPTR *)0) {
  register u_int i;
  register VECLST *cur;

  for (i=0,cur=vptr->vcl->prv;i<vptr->numvec;i++) {
    register VECLST *tofree;
    register void *payload;

    tofree=cur;
    payload=tofree->payload;
    cur=tofree->prv;
    (void) free(tofree);
    if ((payload!=(void *)0)&&(freepayload!=(void *(*)(void*))0))
      (void) freepayload(payload);
    }
  (void) free(vptr);
  vptr=(VPTR *)0;
  }
return vptr;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to count number of element	*/
/*	within an ordered list.			*/
/*						*/
/************************************************/
unsigned int vec_sizelstlst(void **lptr)

{
register unsigned int num;

num=(unsigned int)0;
if (lptr!=(void **)0) {
  for (;*lptr!=(void *)0;lptr++,num++);
  }
return num;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to add a payload within	*/
/*	an ordered list				*/
/*						*/
/************************************************/
void **vec_addlstlst(void **lptr,void *payload)

{
int num;

num=0;
if (lptr!=(void **)0) {
  num=vec_sizelstlst(lptr);
  lptr=(void **)realloc(lptr,(num+2)*sizeof(void *));
  }
else
  lptr=(void **)calloc(2,sizeof(void *));
lptr[num]=payload;
lptr[num+1]=(void *)0;
return lptr;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to merge to ordereded list	*/
/*	second list array definition is freed	*/
/*						*/
/************************************************/
void **vec_mrglstlst(void **lptr,void **ladd)

{
if (ladd!=(void **)0) {
  register unsigned int t1;
  register unsigned int t2;

  if (lptr==(void **)0)
    lptr=(void **)calloc(1,sizeof(void *));
  t1=vec_sizelstlst(lptr);
  t2=vec_sizelstlst(ladd);
  lptr=(void **)realloc(lptr,(t1+t2+1)*sizeof(void *));
  (void) memcpy(lptr+t1,ladd,(t2+1)*sizeof(void *));
  (void) free(ladd);
  }
return lptr;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to remove one payload from	*/
/*	within an ordered list			*/
/*	Return true if success, false otherwise	*/
/*						*/
/************************************************/
int vec_rmlstlst(void **lptr,void *payload)

{
int success;

success=false;
if (lptr!=(void **)0) {
  for (;*lptr!=(void *)0;lptr++) {
    if (success==false) {
      if (*lptr==payload) {
	success=true;
	lptr--;
	}
      }
    else 
      *lptr=*(lptr+1);
    }
  }
return success;
}
/*

*/
/************************************************/
/*						*/
/*	Procedure to remove all payload from	*/
/*	an ordered list.			*/
/*						*/
/************************************************/
void **vec_freelstlst(void **lptr,void *(*freepayload)(void *))

{
if (lptr!=(void **)0) {
  register void **p;

  for (p=lptr;*p!=(void *)0;p++) {
    if (freepayload!=(void *(*)(void*))0)
      *p=freepayload(*p);
    }
  (void) free(lptr);
  lptr=(void **)0;
  }
return lptr;
}
