#----------------------------------------------------------------------
#Vzgot void configuration file to be used by vzgot container
#----------------------------------------------------------------------

install -m0664 -o root -g utmp /dev/null /run/utmp

halt -B  # for wtmp

msg "Seeding random number generator..."
seedrng || true

msg "Setting up loopback interface..."
ip link set up dev lo

[ -r /etc/hostname ] && read -r HOSTNAME < /etc/hostname
if [ -n "$HOSTNAME" ]; then
  msg "Setting up hostname to '${HOSTNAME}'..."
  hostname ${HOSTNAME}
  else
  msg_warn "Didn't setup a hostname!"
  fi

if [ -n "$TIMEZONE" ]; then
  msg "Setting up timezone to '${TIMEZONE}'..."
  ln -sf "/usr/share/zoneinfo/$TIMEZONE" /etc/localtime
  fi
