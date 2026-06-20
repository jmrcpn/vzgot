# Template to customize the container status layout to
# your own needs (see the variable list at the bottom of the page)
#=========================================================================
# Start of display template
>                [ $CNAME:______________ VZGOT CONTAINER STATUS ]
>--------------------------------------------------------------------------
>Load Average     : $LOADAVG:_____________
>Time Up          : $UPTIME:_________    | PID:$PID:____ | Users: $USERS:
>--------------------------------------------------------------------------
>HOST RESOURCES
>CPU              : $ONL: $HMODEL:
>MEMORY           : $HMEM:               | SWAP     : $HSWAP:
>Mem Pressure:    : $HMPRESS:            | OOM Kills: $HOOMKILL:
>==========================================================================
>VZGOT Metrics                           | cgroup v2 Raw Data
>--------------------------------------------------------------------------
>Priority (PWRCPU): $PWRCPU:             | Weight   :    $CHWRATIO:
>Affinity (NUMCPU): $NUMCPU:             | CPUs list:    $LSTASS:
>Ceiling (MAXCPU) : $MAXCPU:             | Max Usage:    $CEILING:
>CPUs idle average: $CPUIDL:             | Busy time:  $CPUUSED:
>Throttled count  : $THROTTLE:           | Wait time:  $CPUWAIT:
>
>==========================================================================
>CONTAINER RESOURCES
>RAM (MEMMAX)     : $MEMMAX:  (% of Host)| Memory Used/max:  $CMEM:
>SWAP (SWAPMAX)   : $SWAPMAX:     "      | Swap Used/max  :  $CSWAP:
>Mem Pressure     : $CMPRESS:     "      | OOM Kills      :  $COOMKILL: 
>==========================================================================
>NETWORK
>--------------------------------------------------------------------------
>Host             : $HNAME:
>interface        : $HINT:               | IP             : $HIP:
>net I/O (RX / TX): $HTRXTX:             | $HSRXTX:
>--------------------------------------------------------------------------
>Container        : $CNAME:
>interface        : $CINT:               | IP             : $CIP:
>net I/O (RX / TX): $CTRXTX:             | $CSRXTX:
>--------------------------------------------------------------------------
#end of dusplay template
#list of variables and display format
$CEILING=<%s>			# Container/Host CPUs Ceiling ratio
$CHWRATIO=<%-10s>		# Container/Host CPUs weight ratio
$CINT=<%-6s>			# Container Network interface name
$CIP=<%s>			# Container IP number
$CMEM=<%s>			# Container memory ratio
$CMPRESS=<%6.2lf%%  >		# Container memory pressure
$CNAME=<%-21s>			# Container Fully Aualified Domai Nname
$COOMKILL=<%llu>		# Container number of OOM kill
$CPUIDL=<%6.2f%% >		# Percentage of time the CPUs are idle
$CPUUSED=<%-15s>		# CPU busy real accumlated time 
$CPUWAIT=<%-15s>		# CPU stuck in waiting time (too busy)
$CTRXTX=<%-21s>			# Container Total bandwidth
$CSRXTX=<%s>			# Container bandwidth speed
$CSWAP=<%-10s>			# Container SWAP used/max value
$LOADAVG=< %-21s>		# Container load average value
$LSTASS=<%-10s>			# List of assigned CPUs on container
$HINT=<%s>			# Host Network interface name
$HIP=<%s>			# Host IP number
$HMEM=<  %-11s>			# Host memory ratio
$HMPRESS=<%6.2lf%%  >		# Host memory pressure
$HMODEL=<'%s'>			# Host CPU model
$HNAME=<%s>			# Host Fully Qualified Domain Name
$HOOMKILL=<%llu>		# Host number of OOM kill
$HSWAP=<%s>			# Host SWAP used/max value
$HTRXTX=<%-21s>			# Host Total bandwidth
$HSRXTX=<%s>			# Host bandwidth speed
$MAXCPU=<%6.2f%% >		# Container config variable $MAXCPU
$MEMMAX=<%6.2f%% >		# Container config variable $MEMMAX
$NUMCPU=<%6.2f%% >		# Container configuration variable $NBRCPU
$ONL=<%d>			# Number of CPU online, available on Host
$PID=<% 9u>			# Container PID	
$PIDMAX=<%6.2f%% >		# Container configuration variable $PIDMAX
$PWRCPU=<%6.2f%% >		# Container configuration variable $PWRCPU
$SWAPMAX=<%6.2f%%  >		# Container configuration variable $SWAPMAX
$THROTTLE=<% 7d   >		# number of throttle events
$USERS=<% 3u    >		# Container number of user	
$UPTIME=<%-15s>			# Container uptime
#--------------------------------------------------------------------------
