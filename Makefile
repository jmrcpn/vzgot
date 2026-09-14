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
	   @ - rm -fr *.tar.gz

#===================================================================
#to generate distribution packages
dodist	:
	   @ $(MAKE) -s $(DIST_TAG)
	   @ echo "$(DIST_TAG) package available within $(APPNAME)_$(DEBVER)"

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

DIST_TAG= $(shell . /etc/os-release && echo $$ID)
DIST_ID	= $(shell ./packagers/get_dist_id.sh)
LOCREPO	=  ./$(APPNAME)_$(VERSION).$(DIST_TAG)
APLR	=  $(APPNAME)-$(VERSION)
#--------------------------------------------------------------------
#Makefile sub-function
-include	./Makefile.vers
include		./Makefile.doapk
include		./Makefile.dodpkg
include		./Makefile.doebuild
include		./Makefile.dorpm
include		./Makefile.doxbps
include		./Makefile.install
#--------------------------------------------------------------------
#SAFE Makefile Management
-include	./Makefile.safe
#--------------------------------------------------------------------
