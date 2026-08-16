#--------------------------------------------------------------------
#to compile opensrc clement application
#--------------------------------------------------------------------
debug	:
	  @ $(MAKE)						\
		OPTIME="-O0 -ggdb -fsanitize=address"		\
		VZDEBUG="-DVZGOT_DBG"				\
		exe

prod	:
	  @ $(MAKE)						\
		OPTIME="-ggdb3 -D_FORTIFY_SOURCE=2 -O2"		\
		exe
	  @ strip $(EXE) $(STA)

#version to be used with gdb in live test system
withgdb	:
	  @ $(MAKE)						\
	        OPTIME="-O0 -ggdb -fsanitize=address"		\
		exe

exe	: $(VZLIB)
	  $(MAKE) $(EXE) $(STA)


clean	:
	  @ - rm -fr *.o $(EXE) $(STA) *~ 

#--------------------------------------------------------------------
#Executable file to do
STA	=							\
	vzmon							\

EXE	=							\
	vzgot							\
	vzstat

OBJS	=							\
	vzmon.o							\
	vzgot.o							\
	vzstat.o

VZLIB= ../lib/vzlb.a
#--------------------------------------------------------------------
#dependencies

$(EXE)	:  $(OBJS) $(VZLIB)
	   @ $(LD) $(LDFLAGS) -o $@ $@.o $(LIBS)

$(STA)	:  $(OBJS) $(VZLIB)
	   $(LD) $(LDFLAGS) -pie -o $@ $@.o $(LIBS)

#--------------------------------------------------------------------
vzmon.o	:							\
	   vzmon.c

vzgot.o	:	  						\
	   ../lib/lowapl.h					\
	   ../lib/lowtyp.h					\
	   ../lib/utlapl.h					\
	   ../lib/utlapp.h					\
	   ../lib/utlprc.h					\
	   ../lib/utlsys.h					\
	   ../lib/utlvec.h					\
	   ../lib/subcfg.h					\
	   ../lib/subprc.h					\
	   ../lib/unicfg.h					\
	   ../lib/unilck.h					\
	   ../lib/unicnt.h					\
	   ../lib/unimon.h					\
	   vzgot.c

vzstat.o:	  						\
	   ../lib/lowapl.h					\
	   ../lib/lowtyp.h					\
	   ../lib/utlapl.h					\
	   ../lib/utlprc.h					\
	   ../lib/utlvec.h					\
	   ../lib/subcfg.h					\
	   ../lib/unicfg.h					\
	   ../lib/unilck.h					\
	   ../lib/unicnt.h					\
	   vzstat.c

#--------------------------------------------------------------------
#compiler and loader options
CC=gcc $(OPTIME) $(VZDEBUG) -fPIE
LD=$(CC)
LIBS=								\
	$(VZLIB)						\
	$(APPARMOR_LIB)						\
	-lm							\

CFLAGS=								\
	-I .							\
	-I ../lib						\
 	-DDIST_$(DIST)						\
	-DUCD_COMPATIBLE					\
	-DPUBLIC=''						\
	-D_GNU_SOURCE						\
	-fno-omit-frame-pointer					\
	-fPIE							\
	-std=gnu99						\
	-pedantic						\
	-Wall							\
	-Wshadow
#--------------------------------------------------------------------
#detecting if we are in apparmor context
ifneq ($(wildcard /usr/include/sys/apparmor.h),)
APPARMOR_LIB= -lapparmor
endif
#--------------------------------------------------------------------
