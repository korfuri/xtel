/*	
 *   xtel - Emulateur MINITEL sous X11
 *
 *   Copyright (C) 1991-1994  Lectra Systemes & Pierre Ficheux
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *		 
 */
static char rcsid[] = "$Id: dial.c,v 1.17 1998/10/02 15:00:49 pierre Exp $";

/*
 * Composition des numeros telephoniques
 */

/* Contributions:
 *
 *   Michel Fingerhut	IRCAM Paris
 *
 *	- code Ultrix pour les ioctl *		 
 *	- Traitement du fichier de log
 *	- Acces proteges aux services
 *
 *   Pierre Beyssac	SYSECA
 *
 *	- code 386BSD
 *	- traitement de l'hexa dans les chat-scripts
 *	- traitement du Minitel 2
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#ifdef SVR4
#include <sys/mkdev.h>
#endif /* SVR4 */
#include <errno.h>
#ifdef USE_SYSLOG
#include <syslog.h>
#endif /* USE_SYSLOG */

#include "demon.h"
#include "globald.h"

static char nom_lck[256];
extern char type_client;

/*
 * fonction UNDIAL
 */
void myundial (fd)
int fd;
{
    if (unlink (nom_lck) < 0) {
	erreur_a_xtel (nom_lck, errno);
    }

    restore_tty (fd);
    close (fd);
}


/*
 * Fonction DIAL
 */
mydial (telno, device)
char *telno, *device;
{
    char buf[80], erreur;
    struct stat statb;
    int fdlck, fd;

    for (numero_ligne = 0 ; numero_ligne != nb_lignes && definition_lignes[numero_ligne].type_dialer != DIALER_M1 ; numero_ligne++) 
	;
    
    /* Minitel 1 ==> force la ligne */
    if (numero_ligne != nb_lignes) 
	nb_lignes = numero_ligne + 1;
    else
	numero_ligne = 0;

    while (numero_ligne < nb_lignes) {
        /*
	 * Recherche la premiere ligne non deja utilise par un programme UUCP
	 */
	for (;;) {
#ifdef SVR4
	    if (stat (definition_lignes[numero_ligne].nom, &statb) != 0) {
		erreur_a_xtel ("mydial()", errno);
		return (-1);
	    }
	    sprintf (nom_lck, FICHIER_LCK, (unsigned long) major(statb.st_dev), (unsigned long) major(statb.st_rdev), (unsigned long) minor(statb.st_rdev));
#else
	    sprintf (nom_lck, FICHIER_LCK, &definition_lignes[numero_ligne].nom[5]);
#endif /* SVR4 */
#ifdef DEBUG_XTELD
	    log_debug( "ligne= %s, device= %s, lock= %s", definition_lignes[numero_ligne].nom, definition_lignes[numero_ligne].device, nom_lck);
#endif
	    /* Si le lock existe */
	    if (stat (nom_lck, &statb) == 0) {
		if (numero_ligne == nb_lignes-1) {
		    /* Dommage, c'etait la derniere :-( */
		    erreur_a_xtel ("[0] Aucun MODEM disponible !", 0);
		    return (-1);
		}
	    }
	    else {
	      /* Sinon, on verifie que le device corresponde */
	      if (device == NULL || strcmp (device, definition_lignes[numero_ligne].device) == 0)
		break;
	      else {
		if (numero_ligne == nb_lignes - 1) {
#ifdef DEBUG_XTELD
		  log_debug ("%s != %s", device, definition_lignes[numero_ligne].device);
#endif
		  erreur_a_xtel ("[2] Pas de device correspondant !", 0);
		  return -1;
		}
	      }
	    }
	    
	  once_again:
	    numero_ligne++;
	}
	
#ifdef DEBUG_XTELD
	log_debug ("creation de %s", nom_lck);
#endif

	/* on cree un fichier semaphore LCK..ttyxx */
	if ((fdlck = open (nom_lck, O_WRONLY|O_EXCL|O_CREAT, 0644)) < 0) {
	    erreur_a_xtel (nom_lck, errno);
	    return (-1);
	}
	
	/* on ecrit le PID dedans */
	sprintf (buf, "%10d\n", getpid ());
	write (fdlck, buf, strlen (buf));
	close (fdlck);
	
#ifdef DEBUG_XTELD
	log_debug ("Ouverture de la ligne %s", definition_lignes[numero_ligne].nom);
#endif
	/* ouvre la ligne */
	if ((fd = open (definition_lignes[numero_ligne].nom, O_RDWR|O_NDELAY)) < 0) {
	    /* Derniere ligne, on passe l'erreur */
	    if (numero_ligne == nb_lignes-1) {
		erreur_a_xtel (definition_lignes[numero_ligne].nom, errno);
		return (-1);
	    }
	    /* Sinon on essaye la ligne suivante */
	    else {
		unlink (nom_lck);
		goto once_again;	/* aller a, jacta est */
	    }
	}

	/* Init des parametres de la ligne */
	init_tty (fd, definition_lignes[numero_ligne].speed, definition_lignes[numero_ligne].cs, definition_lignes[numero_ligne].parity, definition_lignes[numero_ligne].flags, definition_lignes[numero_ligne].type_dialer);

#ifdef DEBUG_XTELD
	log_debug ("Dialogue Modem...");
#endif

	erreur = do_chat (fd, definition_lignes[numero_ligne].chat, (unsigned long)definition_lignes[numero_ligne].delai, telno, NULL, 0);

	/*
	 * Test de l'erreur en sortie
	 */
	if (!erreur) {
#ifdef ultrix
 	    term.c_cflag &= ~CLOCAL;
 	    ioctl (fd, TCSETA, &term);
#endif
	    return (fd);
	}
	else {
	    /*
	     * Sortie en timeout => efface le fichier semaphore 
	     */
	    
	    if (unlink (nom_lck) < 0) {
		erreur_a_xtel (nom_lck, errno);
	    }
	    close (fd);
	    numero_ligne++;
	}
    }

    /*
     * Erreur connexion !
     */
    erreur_a_xtel ("[1] Erreur de connexion MODEM !", 0);
    close (fd);

    return (-1);
}
