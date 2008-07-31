#!/bin/bash
# $Id: make_xtel_lignes.sh,v 1.5 2001/02/11 19:27:59 pierre Exp $

# Détection des modems

XTEL_LIGNES="/usr/X11R6/lib/X11/xtel/xtel.lignes"
MDMDETECT="/usr/X11R6/bin/mdmdetect"

KERNELV=`uname -r | tr -d "." | cut -c1,2`

if [ $KERNELV -gt 20 ]; then
    TTYNAME=ttyS
else
    TTYNAME=cua
fi

tput init
tput clear
echo
tput rev
echo "Détection des modems et génération du xtel.lignes (kernel `uname -r`)"
tput rmso

M=`$MDMDETECT -q`
if [ "$M" = "" ]; then
    echo "Liste des modems non disponible !"
    exit 1
fi

NEW_XTEL_LIGNES=$XTEL_LIGNES.$$
rm -f $NEW_XTEL_LIGNES

j=0
for i in 0 1 2 3
do
	unset m
	unset MDM_BUILDER

	TTYLINE=${TTYNAME}${i}

	# Test mgetty
	if [ "$TTYNAME" != ttyS ]; then
	    if [ -r /var/run/mg-pid.ttyS${i} ]; then
		MGETTYPID=/var/run/mg-pid.ttyS${i}
	    elif [ -r /var/run/mgetty-pid.ttyS${i} ]; then
		MGETTYPID=/var/run/mgetty-pid.ttyS${i}
	    fi
	    if [ "$MGETTYPID" != "" ]; then
		PID=`cat $MGETTYPID`
		if [ -r /proc/$PID ]; then
		    if [ "`cat /proc/$PID/cmdline | grep mgetty`" != "" ]; then
			TTYLINE=ttyS${i}
		    fi    
		fi	
	    fi
	fi    

	tput clear
	echo
	echo "Détection sur /dev/${TTYLINE}..."
	echo
	echo "Les marques de modem connues sont: "
	im=1
	for i in $M
	do
	    tput rev; echo -n "[$im]"; tput rmso
	    echo " $i"
	    im=`expr $im + 1`
	done

	MAXM=`expr $im - 1`

	echo
	echo -n "Veuillez entrer la marque du modem de 1 à $MAXM (q pour sauter la détection) : "

	read m < /dev/tty

	if [ "$m" != "q" ]; then
	    if [ "$m" != "" ]; then
		# Recherche de la marque
		im=1
		for ii in $M
		do
		    if [ ${im} = $MAXM ]; then
			echo "Pas de marque correspondante, recherche sans marque."
			unset m
			break
		    fi	

		    if [ ${im} = $m ]; then
			m=${ii}
			break
		    fi	
		    im=`expr $im + 1`
		done    

		if [ "$m" != "" ]; then    
		    MDM_BUILDER="-b $m"
		fi    
	    fi    

	    X=`$MDMDETECT $MDM_BUILDER $MDM_LIST ${TTYLINE}`
	    if [ $? = 0 ]; then
		set $X
		echo
		MDM=`echo $1 | sed -e "s/-/ /g"`
		echo "Le modem détecté est de type: $m $MDM"
		echo
		echo -n "Quel votre préfixe d'appel (exemple: 0w) ? "
		read c < /dev/tty	
		    
		echo "# $m $MDM" >> $NEW_XTEL_LIGNES
		echo "modem${j}${2}/dev/${TTYLINE}${2}$3${2}7${2}E${2}$4 atdt$c\\T\\r CONNECT${2}30" | sed -e 's/-/ /g' >> $NEW_XTEL_LIGNES
		echo >> $NEW_XTEL_LIGNES
		j=`expr $j + 1`
	    else
		sleep 1
	    fi
	fi
done

if [ ! -r $NEW_XTEL_LIGNES ]; then
    echo "Aucun modem détecté."
else
    if [ $j -gt 1 ]; then s="s"; fi
    echo "$j modem$s détecté$s, création du $XTEL_LIGNES"
    if [ -f $XTEL_LIGNES ]; then mv ${XTEL_LIGNES} $XTEL_LIGNES.old ; fi
    mv $NEW_XTEL_LIGNES $XTEL_LIGNES
fi
