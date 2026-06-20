// vim: smarttab tabstop=8 shiftwidth=2 expandtab
/************************************************/
/*	to interface vzgot with apparmor	*/
/*						*/
/************************************************/
#ifndef UTLAPP
#define UTLAPP

#include	<stdbool.h>

//define apparmor profile ENV variable
#define APP_PRO "APPARMOR_PROFILE"

//load the apparmor profile
extern _Bool app_load_profile(const char *profile);

//parse and apparmor profile to be stored within kernel
extern _Bool app_parse_profile(const char *profile,_Bool replace);

#endif
