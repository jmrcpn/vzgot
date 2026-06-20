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

withgdb	:
	  @ $(MAKE)                                             \
		OPTIME="-O0 -ggdb -fsanitize=address"		\
		exe

exe	: 
	  $(MAKE) $(EXE)
	
clean	:
	  @ - rm -fr *.o $(EXE) *~ 

#--------------------------------------------------------------------
#Executable file to do
EXE=								\
	tstavg							\
	tstonl							\
	setcpu 

OBJS=								\
	tstavg.o						\
	tstonl.o						\
	setcpu.o						\


VZLIB= ../lib/vzlb.a
#--------------------------------------------------------------------
#dependencies
tstavg.o: $(VZLIB)

$(EXE)	:  $(OBJS) $(VZLIB)
	   @ $(LD) $(LDFLAGS) -o $@ $@.o $(LIBS)
	
#--------------------------------------------------------------------
setcpu.o:							\
	   ../lib/lowtyp.h					\
	   ../lib/utlapl.h					\
	   ../lib/utlprc.h					\
	   ../lib/utlsys.h					\
	   ../lib/subcfg.h					\
	   ../lib/subprc.h					\
	   ../lib/unicnt.h					\
	   ../lib/unilck.h					\
	   setcpu.c

tstavg.o:							\
	   ../lib/lowtyp.h					\
	   ../lib/utlapl.h					\
	   ../lib/utlprc.h					\
	   ../lib/subcfg.h					\
	   ../lib/unicnt.h					\
	   ../lib/unilck.h					\
	   tstavg.c

tstonl.o:							\
	   ../lib/lowtyp.h					\
	   ../lib/utlapl.h					\
	   ../lib/utlprc.h					\
	   ../lib/subcfg.h					\
	   ../lib/unicnt.h					\
	   ../lib/unilck.h					\
	   tstonl.c

#--------------------------------------------------------------------
#compiler and loader options
CC=gcc $(OPTIME) $(VZDEBUG) -fPIE
LD=$(CC)
LIBS=								\
	$(VZLIB)						\
	 -lm
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
