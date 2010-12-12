#! /bin/sh
# Init script for jackfreqd
#
### BEGIN INIT INFO
# Provides:          jackfreqd
# Required-Start:    $syslog $remote_fs $time
# Required-Stop:     $syslog $remote_fs
# Default-Start:     2 3 4 5
# Default-Stop:	     0 1 6
# Short-Description: Start jackfreqd .
### END INIT INFO


PATH=/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/sbin/jackfreqd
NAME=jackfreqd
DESC=jackfreqd
OPTIONS="-P -w -q"
ENABLE="true"

test -x $DAEMON || exit 0

# modify the file /etc/default/jackfreqd if you want to add personal options

[ -f /etc/default/$NAME ] && . /etc/default/$NAME

test "$ENABLE" == "true" || exit 0

set -e

case "$1" in
  start)
	echo -n "Starting $DESC: "
	if [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]
	then
		start-stop-daemon --start --quiet --oknodo --exec $DAEMON -- -d $OPTIONS
	else
		echo "required sysfs objects not found!"
		exit 0
	fi
	echo "$NAME."
	;;
  stop)
	echo -n "Stopping $DESC: "
	start-stop-daemon --stop --quiet --oknodo --exec $DAEMON
	echo "$NAME."
	;;
  restart|force-reload)
	echo -n "Restarting $DESC: "
	start-stop-daemon --stop --quiet --oknodo --exec $DAEMON
	sleep 1
	start-stop-daemon --start --quiet --oknodo --exec $DAEMON -- -d $OPTIONS
	echo "$NAME."
	;;
  *)
	N=/etc/init.d/$NAME
	echo "Usage: $N {start|stop|restart|force-reload}" >&2
	exit 1
	;;
esac

exit 0
