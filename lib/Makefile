#--------------------------------------------------------------------
#to compile vzgot open source library
#--------------------------------------------------------------------

debug	:
	  @ $(MAKE)						\
	        OPTIME="-O0 -ggdb -fsanitize=address"		\
		VZDEBUG="-DVZGOT_DBG"				\
		YVERB="-v"					\
		memlib

prod	:
	  @ $(MAKE)						\
		OPTIME="-ggdb3 -D_FORTIFY_SOURCE=2 -O2" 	\
	        memlib

#version to be used with gdb
withgdb	:
	  @ $(MAKE)						\
	        OPTIME="-O0 -ggdb -fsanitize=address"		\
		YVERB="-v"					\
		memlib

memlib	:
	  @ $(MAKE) $(FAST) $(VZLB)

clean	:
	   @ - rm -f $(VZLB) $(OBJS)
	   @ - rm -f *~

#--------------------------------------------------------------------
#Object file list to generate
OBJS=								\
	unicfg.o unicnt.o unilck.o				\
	subcfg.o subprc.o					\
	utlapl.o utlapp.o utlsys.o utlprc.o utlvec.o 		\
	dbglog.o

VZLB= 								\
       vzlb.a
#--------------------------------------------------------------------
#dependancies
vzlb.a	:  $(OBJS)
	   ar -cr $@ $(OBJS)
	   ranlib $@
#--------------------------------------------------------------------
#'C' Definition

unicfg.o:							\
	  lowtyp.h						\
	  dbglog.h						\
	  utlapl.h utlsys.h utlvec.h				\
	  subcfg.h subprc.h					\
	  unicfg.h unicfg.c	

uniclk.o:							\
	  lowtyp.h						\
	  dbglog.h						\
	  utlapl.h						\
	  subprc.h						\
	  unilck.h unilck.c

unicnt.o:							\
	  lowtyp.h						\
	  dbglog.h						\
	  utlapl.h utlsys.h					\
	  subcfg.h subprc.h					\
	  unicnt.h unicnt.c

subcfg.o:							\
	  lowtyp.h						\
	  dbglog.h						\
	  utlapl.h utlprc.h					\
	  subcfg.h subcfg.c

subprc.o:							\
	  lowtyp.h						\
	  dbglog.h						\
	  utlapl.h utlprc.h utlsys.h				\
	  subprc.h subprc.c

utlapp.o:							\
	  dbglog.h						\
	  utlapp.h utlapp.c

utlapl.o:							\
	  lowtyp.h						\
	  dbglog.h						\
	  utlapl.h utlapl.c

utlsys.o:							\
	  lowtyp.h						\
	  dbglog.h						\
	  utlsys.h utlsys.c

utlprc.o:							\
	  lowtyp.h						\
	  dbglog.h						\
	  utlprc.h utlprc.c

utlvec.o:							\
	  lowtyp.h						\
	  dbglog.h						\
	  utlvec.h utlvec.c

dbglog.o:							\
	  dbglog.h dbglog.c

kernsrc	:
	  @ ln -nsf						\
		/usr/src/kernels/$(KSRCVER)/tools/include	\
		./kernsrc

#--------------------------------------------------------------------
#compiler options
YFLAGS= -p $* $(YVERB)
CC=gcc $(OPTIME) $(VZDEBUG) $(POSTGRESQL) $(MYSQL)
CFLAGS=								\
	-I .							\
 	-DDIST_$(DIST)						\
	-DUCD_COMPATIBLE					\
	-DPUBLIC=''						\
	-D_GNU_SOURCE						\
	-fno-omit-frame-pointer					\
	-fPIE							\
	-std=gnu99						\
	-pedantic						\
	-Wall							\
	-Wshadow						\

FAST    = "-j `(/usr/bin/getconf _NPROCESSORS_ONLN)`"
KSRCVER	= $(shell cat /etc/kernel-devel)
#--------------------------------------------------------------------
ifneq ($(wildcard /usr/include/sys/apparmor.h),)
CFLAGS += -DHAVE_APPARMOR
endif
#--------------------------------------------------------------------
