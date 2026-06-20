#--------------------------------------------------------------------
#To make a debugging executable
prod								\
debug								\
withgdb								\
	:  
	   @ for i in $(SUBDIRS) ;				\
             do							\
	     echo "Doing: $$i $@";				\
	     $(MAKE) -C $$i $@ ;				\
	     RETVAL=$$? ;					\
	     if [ $$RETVAL != 0 ] ; then 			\
		exit $$RETVAL ;					\
		fi ;						\
             done

#To clean distribution
clean	:
	   @ for i in $(SUBDIRS) ;				\
             do							\
	     echo "Doing now \"$$i $@\"" ;			\
	     $(MAKE) -s  -C $$i $@ ;				\
             done			
	   @ - rm -fr spec *.tar.gz


#===================================================================
#all support information
.PHONY:	clean
#--------------------------------------------------------------------
#version management
APPNAME	= vzgot

SUBDIRS	=							\
	  lib							\
	  app							\
	  utilities

#--------------------------------------------------------------------
#Makefile sub-function
include		./Makefile.vers
include		./Makefile.dorpm
#--------------------------------------------------------------------
#SAFE Makefile Management
-include	./Makefile.safe
#--------------------------------------------------------------------
