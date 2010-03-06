#!/bin/sh
# $Id: connect_iminitel.sh,v 1.2 2001/02/11 00:16:52 pierre Exp $
IMINITEL_FILE=/tmp/.iminitel

case "$1" in
    start)
	# Test lock
	if [ -r ${IMINITEL_FILE} ]; then
	    exit 1
	fi

	# Pose le lock
	echo -n > ${IMINITEL_FILE}

	# Appel serveur 3622
	/usr/sbin/pppd call iminitel ipparam iminitel
	exit $?
	;;

    stop)
	# Si ppp est encore actif, on coupe
	if [ -r ${IMINITEL_FILE} ]; then
	    . ${IMINITEL_FILE}
	    # Tue de demon pppd
	    kill `cat /var/run/${IMINITEL_INTERFACE}.pid`
	fi    
	;;

    *)
	exit 1
	;;
esac
