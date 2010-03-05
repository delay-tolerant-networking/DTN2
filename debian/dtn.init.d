#!/bin/sh
### BEGIN INIT INFO
# Provides:          dtnd
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Delay-Tolerant Network server
### END INIT INFO
#/etc/init.d/dtnd
# Modified by: Francesco Vollero <raven@lilik.it>

# dtnd		Start the DTN daemon
#
# The variables below are NOT to be changed.  They are there to make the
# script more readable.
#
# This script was modified from the apache rc script

NAME=dtnd
DAEMON=/usr/bin/$NAME
CONTROL=/usr/bin/$NAME-control
ARGS="-d -o /var/dtn/dtnd.log"
PIDFILE=/var/run/$NAME.pid
SSD=`which start-stop-daemon`
SSDARGS="-u dtn -c dtn:root --exec $DAEMON"
ENV="env -i LANG=C PATH=/bin:/usr/bin:/usr/local/bin"

test $DEBIAN_SCRIPT_DEBUG && set -v -x

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
    echo "Starting DTN daemon..."
    should_start
    $SSD $SSDARGS --start -- $ARGS
    ;;

  start_tidy)
    should_start
    echo "Starting DTN daemon (tidy mode)..."
    $SSD $SSDARGS --start -- $ARGS -t
    ;;

  stop)
    echo "Stopping DTN daemon..."
    $CONTROL stop || true
    echo "Making sure DTN daemon stops..."
    $SSD $SSDARGS --stop || true
    ;;

  restart)
    echo "Stopping DTN daemon..."
    $CONTROL stop || true
    echo "Making sure DTN daemon stops..."
    $SSD $SSDARGS --stop || true
    echo "Restarting DTN daemon..."
    $SSD $SSDARGS --start -- $ARGS
    ;;

  logrotate)
    echo "Rotating logs for DTN daemon..."
    $CONTROL logrotate
    ;;

  *)
    echo "Usage: /etc/init.d/$NAME {start|start_tidy|stop|restart|logrotate}"
    exit 1
    ;;
esac
