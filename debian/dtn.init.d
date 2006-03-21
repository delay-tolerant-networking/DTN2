#!/bin/bash
#
# dtnd		Start the DTN daemon
#
# The variables below are NOT to be changed.  They are there to make the
# script more readable.
#
# This script was modified from the apache rc script

NAME=dtnd
DAEMON=/usr/bin/$NAME
CONTROL=/usr/bin/$NAME-control
ARGS="-d -o /var/log/dtnd.log"
PIDFILE=/var/run/$NAME.pid
# note: SSD is required only at startup of the daemon.
SSD=`which start-stop-daemon`
ENV="env -i LANG=C PATH=/bin:/usr/bin:/usr/local/bin"

trap "" 1

cd /

should_start() {
    if [ ! -x $DAEMON ]; then
	echo "$NAME is not executable, not starting"
	exit 0
    fi
}

case "$1" in
  start)
    echo -n "Starting DTN daemon..."
    should_start
    start-stop-daemon --start --exec $DAEMON -- $ARGS
    ;;

  start_tidy)
    should_start
    echo -n "Starting DTN daemon (tidy mode)..."
    $ENV $DAEMON $ARGS -t > /dev/null 2>/dev/null &
    ;;

  stop)
    echo -n "Stopping DTN daemon..."
    $CONTROL stop
    ;;

  restart)
    echo -n "Restarting DTN daemon..."
    $CONTROL stop
    $ENV $DAEMON $ARGS > /dev/null 2>/dev/null &
    ;;

  logrotate)
    echo -n "Rotating logs for DTN daemon..."
    $CONTROL logrotate
    ;;

  *)
    echo "Usage: /etc/init.d/$NAME {start|start_tidy|stop|restart|logrotate}"
    exit 1
    ;;
esac
