#!/bin/sh
# $Id: connect_iminitel.sh,v 1.2 2001/02/11 00:16:52 pierre Exp $

case "$1" in
    start)
	# Appel serveur 3622
	/usr/sbin/pppd call iminitel ipparam iminitel
	exit $?
	;;

    stop)
	# Si ppp est encore actif, on coupe
	if [ -r /var/run/ppp-iminitel.pid ]; then
	    # Tue le demon pppd
	    kill `head -1 /var/run/ppp-iminitel.pid`
	fi    
	;;

    *)
	exit 1
	;;
esac
