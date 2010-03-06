#!/bin/sh
# $Id: install_iminitel.sh,v 1.4 2001/02/13 15:20:47 pierre Exp $
echo "Configuration des composants I-Minitel..."
if [ ! -r /etc/ppp ]; then
    echo "Package pppd non installé."
    exit 1
fi

# Droits
echo "Droits d'accès..."
chmod 600 /etc/ppp/peers/iminitel /etc/ppp/chat-iminitel

# Ajout dans ip-up et ip-down
cd /etc/ppp
for i in up down
do
    echo -n "Mise à jour ip-${i}..."

    if [ "`grep Minitel ip-${i}`" != "" ]; then
	echo "déja configuré."
	continue
    fi

    cat ip-${i} | while read line
    do
	if [ "`echo $line | grep if${i}-post`" != "" ]; then
	    cat <<EOF >> /tmp/ip-${i}.$$
# For I-Minitel, don't execute if${i}-post
if [ -x /etc/ppp/ip-${i}.iminitel -a "\$6" = "iminitel" ]; then
    /etc/ppp/ip-${i}.iminitel \$*
else
    $line
fi
EOF
	else
	    echo $line >> /tmp/ip-${i}.$$
	fi
    done
    cp ip-${i} ip-${i}.no_xtel
    cp /tmp/ip-${i}.$$ ip-${i}
    rm -f /tmp/ip-${i}.$$
    echo OK
done
