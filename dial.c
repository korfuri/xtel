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

static int try_lock (char *lock_file);

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
    int fd;

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
	while (numero_ligne < nb_lignes) {
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
	    /* On verifie que le device corresponde */
	    if (device != NULL && strcmp (device, definition_lignes[numero_ligne].device) != 0) {
		/* sinon on passe à la ligne suivante */
		if (numero_ligne == nb_lignes - 1) {
#ifdef DEBUG_XTELD
		  log_debug ("%s != %s", device, definition_lignes[numero_ligne].device);
#endif
		  erreur_a_xtel ("[2] Pas de device correspondant !", 0);
		  return -1;
		}
		numero_ligne++;
		continue;
	    }
	    /* Si c'est le bon device, on tente du poser le lock */
	    if (try_lock( nom_lck ) < 0) {
		/* le modem est occupé... on passe au suivant */
		numero_ligne++;
	    }
	    else {
		break;
	    }
	}
	if (numero_ligne >= nb_lignes) {
	    /* Dommage, c'etait la derniere :-( */
	    erreur_a_xtel ("[0] Aucun MODEM disponible !", 0);
	    return (-1);
	}
	
#ifdef DEBUG_XTELD
	log_debug ("Ouverture de la ligne %s", definition_lignes[numero_ligne].nom);
#endif
	/* ouvre la ligne */
	if (definition_lignes[numero_ligne].tempo > 0) {
	    /*
	     * Pour les vieux modems, dans certains cas, il faut réinitialiser le
	     * port série avant de pouvoir communiquer avec le modem.
	     * Ceci est effectué en ouvrant puis refermant le device avec un certain
	     * délai entre chaque opération.
	     */
	    fd = open (definition_lignes[numero_ligne].nom, O_RDWR|O_NDELAY);
	    usleep(definition_lignes[numero_ligne].tempo);
	    close(fd);
	}
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
	init_tty (fd, definition_lignes[numero_ligne].speed, definition_lignes[numero_ligne].cs, definition_lignes[numero_ligne].parity, definition_lignes[numero_ligne].flags, definition_lignes[numero_ligne].type_dialer, definition_lignes[numero_ligne].tempo);

#ifdef DEBUG_XTELD
	log_debug ("Dialogue Modem...");
#endif

	erreur = do_chat (fd, definition_lignes[numero_ligne].chat, (unsigned long)definition_lignes[numero_ligne].delai, definition_lignes[numero_ligne].tempo, telno, NULL, 0);

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
	}

    once_again:
	numero_ligne++;
    }

    /*
     * Erreur connexion !
     */
    erreur_a_xtel ("[1] Erreur de connexion MODEM !", 0);
    close (fd);

    return (-1);
}

/* Verrouille la ligne serie (retourne 0 si ok, -1 si erreur) */
static int try_lock (char *lock_file)
{
  char buf[256];
  struct stat statb;
  int fd, n;
  pid_t pid;

  /* Test du lock */
  do {
      fd = open( lock_file, O_CREAT | O_EXCL | O_WRONLY, 0644 );
      if (fd < 0) {
	  /* erreur lors de la création du fichier de lock */
	  if (errno == EEXIST) {
	      /* le fichier existe : voir si le programme qui l'a créé existe toujours */
	      fd = open( lock_file, O_RDONLY );
	      if (fd >= 0) {
		  n = read( fd, buf, 11 );
		  close( fd );
		  if (n > 0) {
		      buf[n] = '\0';
		      pid = atoi(buf);
		      /* teste l'existence du processus ayant créé le fichier */
		      if (pid == 0 || kill(pid,0) == -1 && errno == ESRCH) {
			  if (unlink(lock_file) == 0) {
#ifdef DEBUG_XTELD
			      log_debug ("Removed stale lock %s (pid %d)", lock_file, pid);
#endif
			      /* et on retente la création dudit fichier */
			      fd = -1;
			  }
			  else {
#ifdef DEBUG_XTELD
			      log_debug ("Can't remove stale lock %s", lock_file);
#endif
			      return -1;
			  }
		      }
		      else {
#ifdef DEBUG_XTELD
			  char *line = strrchr( lock_file, '/' )+6; /* après le /LCK.. */
			  log_debug ("Device %s is already locked by pid %d", line, pid);
#endif
			  return -1;
		      }
		  }
		  else {
#ifdef DEBUG_XTELD
		      log_debug ("Can't read pid from lock file %s", lock_file);
#endif
		      return -1;
		  }
	      }
	      else if (errno != ENOENT) {
		  /* il ne vient pas d'être effacé par un autre programme */
		  /* c'est donc un vrai problème */
#ifdef DEBUG_XTELD
		  log_debug ("%s: %s", lock_file, strerror(errno));
#endif
		  return -1;
	      }
	  }
	  else {
	      /* fichier impossible à créer */
#ifdef DEBUG_XTELD
	      log_debug ("Can't create lock file %s (%s)", lock_file, strerror(errno));
#endif
	      return -1;
	  }
      }
  } while (fd < 0);

  /* Le lock est posé ; il faut y inscrire le pid de ce programme */
  sprintf (buf, "%10d\n", getpid());
  if (write (fd, buf, 11) != 11) {
#ifdef DEBUG_XTELD
      log_debug ("Error writing to file %s (%s)", lock_file, strerror(errno));
#endif
      close (fd);
      return -1;
  }
#ifdef DEBUG_XTELD
  log_debug ("fichier lock %s cree", nom_lck);
#endif

  close (fd);
  return 0;
}

