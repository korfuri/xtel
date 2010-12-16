/*	
 *   xtel - Emulateur MINITEL sous X11
 *
 *   Copyright (C) 1991-1996  Lectra Systemes & Pierre Ficheux
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
 */
static char rcsid[] = "$Id: config.c,v 1.14 1998/10/16 08:49:01 pierre Exp $";

/*
 * fonctions de lecture de la configuration
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef USE_SYSLOG
#include <syslog.h>
#endif /* USE_SYSLOG */
#ifdef NO_TERMIO
#include <sgtty.h>
#else
#ifdef USE_TERMIOS
#include <sys/ioctl.h>
#include <termios.h>
#else
#include <termio.h>
#endif /* USE_TERMIOS */
#endif /* NO_TERMIO */

#include "demon.h"
#include "globald.h"

static char buf[2048], buf1[2048];
static Boolean flag_old_config;

struct couple {
    char *chaine;
    int valeur;
};

static struct couple vitesses_possibles[] = {
    {"50",	B50},
    {"75",	B75},
    {"110", 	B110},
    {"134",	B134},
    {"150",	B150},
    {"200",	B200},
    {"300", 	B300},
    {"600",	B600},
    {"1200",	B1200},
    {"1800", 	B1800},
    {"2400",	B2400},
    {"4800",	B4800},
    {"9600",	B9600},
    {"19200", 	B19200},
    {"38400",	B38400}
};

static struct couple tailles_possibles[] = {
    {"5", CS5},
    {"6", CS6},
    {"7", CS7},
    {"8", CS8}
};

static struct couple parites_possibles[] = {
    {"E", PAIR},
    {"O", IMPAIR},
    {"N", SANS}
};

static int valeur_couple (couples, nb_couples, chaine, valeur_defaut)
struct couple couples[];
int nb_couples;
char *chaine;
int valeur_defaut;
{
    register int i;

    for (i = 0 ; i != nb_couples ; i++) {
	if (strcmp (couples[i].chaine, chaine) == 0)
	    return couples[i].valeur;
    }

    return valeur_defaut;
}

static int get_separator (char *s, char *sep)
{
  int r;
  register char *p = s;

#ifdef DEBUG_XTELD
  log_debug ("get_separator: *p = %c %x", *p, *p);
#endif

  /* Cas particulier des lignes @pipe/@tcp de xtel.services */
  if (*p == '@') {
    *sep = ',';
    return 0;
  }
  
  while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) {
#ifdef DEBUG_XTELD
    log_debug ("get_separator: *p = %c", *p);
#endif
    p++;
  }

  if (*p) {
    *sep = *p;
    r = 0;
  }
  else
    r = -1;

#ifdef DEBUG_XTELD
  log_debug ("get_separator: buf= %s sep= %c", s, *sep);
#endif
  
  return r;
}

/*
 * lecture des services disponibles
 */
lecture_services ()
{
    FILE *fp;
    int i = 0;
    char sep[2] = {0, 0};

    if ((fp = fopen (FICHIER_DEFINITION_SERVICES, "r")) == NULL) {
      sprintf (buf, "Erreur a l'ouverture du fichier %s", FICHIER_DEFINITION_SERVICES);
      log_err (buf);
      return -1;
    }

#ifdef DEBUG_XTELD
    log_debug ("==> Lecture des services <==");
#endif

    /* lecture des services */
    while (fgets (buf, sizeof(buf), fp) != NULL && i < MAX_SERVICES) {

	if (buf[0] == '#' || buf[0] == '\n')
	    continue;
	buf[strlen(buf)-1] = 0;

	/* Lecture separateur */
	if (sep[0] == 0) {
	  if (get_separator (buf, sep) < 0) {
	    log_err ("lecture_services: erreur lecture separateur");
	    fclose(fp);
	    return -1;
	  }
	}
	
	strcpy (buf1, next_token (buf, sep));

	if (!flag_old_config) {
	    definition_services[i].device = strdup (buf1);
	    strcpy (buf1, next_token (NULL, sep));
	}
	else
	    definition_services[i].device = NULL;

	definition_services[i].nom_service = strdup (buf1);
	strcpy (buf1, next_token (NULL, sep));
       	definition_services[i].nom_uucp = strdup (buf1);

	/* lecture des autorisations */
	strcpy (buf1, next_token (NULL, sep));

	definition_services[i].autorisations = strdup (buf1);

#ifdef DEBUG_XTELD1
	if (!flag_old_config)
	    log_debug ("SERVICES: %s %s %s %s", definition_services[i].device, definition_services[i].nom_service, definition_services[i].nom_uucp, definition_services[i].autorisations);
	else
	    log_debug ("SERVICES: %s %s %s", definition_services[i].nom_service, definition_services[i].nom_uucp, definition_services[i].autorisations);
#endif
	i++;
    }
    fclose (fp);

    return (i);
}

/*
 * lecture des lignes disponibles
 */
lecture_configuration_lignes ()
{	
    FILE *fp;
    register int i;
    char *p, sep[3] = {0, 0, 0};

    if ((fp = fopen (FICHIER_DEFINITION_LIGNES, "r")) == NULL) {
      sprintf (buf, "Erreur a l'ouverture du fichier %s", FICHIER_DEFINITION_LIGNES);
      log_err (buf);
      return (-1);
    }

#ifdef DEBUG_XTELD
    log_debug ("==> Lecture des lignes <==");
#endif

    /* lecture des definition de lignes */
    i = 0;
    while (fgets (buf, sizeof(buf), fp) != 0 && i < MAX_LIGNES) {

	if (buf[0] == '#' || buf[0] == '\n')
	    continue;

	/* Lecture separateur */
	if (sep[0] == 0) {
	  if (get_separator (buf, sep) < 0) {
	    log_err ("lecture_configuration_lignes: erreur lecture separateur");
	    fclose(fp);
	    return -1;
	  }
	}

	/* 
	 *  Si la ligne commence par un '/', on considere que l'on a un fichier
	 *  utilisant l'ancienne structure
	 */
	if (buf[0] == '/') {
#ifdef DEBUG_XTELD
	    log_debug ("Ancienne syntaxe !");
#endif
	    flag_old_config = True;
	    definition_lignes[i].device = NULL;
	    definition_lignes[i].speed = B1200;
	    definition_lignes[i].cs = CS7;
	    definition_lignes[i].parity = PAIR;
	}

	strcpy (buf1, next_token (buf, sep));

	if (!flag_old_config) {
	    definition_lignes[i].device = strdup (buf1);
	    strcpy (buf1, next_token (NULL, sep));
	}

	/* Stocke le nom symbolique et lit les flags eventuels */
	if ((p = strchr (buf1, ':'))) {
	    *p = 0;
	    definition_lignes[i].nom = strdup (buf1);
	    if (!strcmp (p+1, "rtscts")) {
		definition_lignes[i].flags = FLAG_RTS_CTS;
	    }
	}
	else
	    definition_lignes[i].nom = strdup (buf1);

	strcpy (buf1, next_token (NULL, sep));

	/* Lecture vitesse + taille caractere + parite */
	if (!flag_old_config) {
	    definition_lignes[i].speed = valeur_couple (vitesses_possibles, 15, buf1, B1200);
	    strcpy (buf1, next_token (NULL, sep));
	    /* Nb bits */
	    definition_lignes[i].cs = valeur_couple (tailles_possibles, 4, buf1, CS7);
	    strcpy (buf1, next_token (NULL, sep));
	    /* Parite */
	    definition_lignes[i].parity = valeur_couple (parites_possibles, 3, buf1, PAIR);
	    strcpy (buf1, next_token (NULL, sep));
	}

	/*
         * Cas particulier: la ligne utilise un Minitel 1/2 comme modem... 
	 * (le chat-script est defini a l'avance)
	 */
	if (strcmp (buf1, "minitel2") == 0) {
	    definition_lignes[i].chat = (char *)calloc (1, strlen (CHAT_M2) + 1);
	    strcpy (definition_lignes[i].chat, CHAT_M2);
	    definition_lignes[i].type_dialer = DIALER_M2;
	}
	else if (strcmp (buf1, "minitel1") == 0) {
	    definition_lignes[i].chat = (char *)calloc (1, strlen (CHAT_M1) + 1);
	    strcpy (definition_lignes[i].chat, CHAT_M1);
	    definition_lignes[i].type_dialer = DIALER_M1;	    
	    flag_m1 = 1;
	}
	else {
	    definition_lignes[i].chat = (char *)calloc (1, strlen (buf1) + 1);
	    strcpy (definition_lignes[i].chat, buf1);
	    definition_lignes[i].type_dialer = DIALER_MODEM;	    
	}

	/*
	 * Ajoute NL à la liste des séparateurs pour autoriser une fin de ligne
	 * après le champ délai
	 */
	sep[1] = '\n';
	definition_lignes[i].delai = atoi (next_token (NULL, sep));

	/*
	 * colonne 8 = tempo entre 2 ioctls (pour vieux modems)
	 * 0 par défaut, i.e. colonne non présente.
	 * saisie en millisecondes, stockage en microsecondes.
	 */
	p = next_token (NULL, "\n");
	if (isdigit(*p))
	    definition_lignes[i].tempo = atoi(p)*1000;
	else
	    definition_lignes[i].tempo = 0;

#ifdef DEBUG_XTELD
	if (!flag_old_config)
	    log_debug ("LIGNES: %s %s %d %d %d >%s< %d (%d)", definition_lignes[i].device, definition_lignes[i].nom, definition_lignes[i].speed, definition_lignes[i].cs, definition_lignes[i].parity, definition_lignes[i].chat, definition_lignes[i].delai, definition_lignes[i].tempo);
	else
	    log_debug ("LIGNES: %s %s %d", definition_lignes[i].nom, definition_lignes[i].chat, definition_lignes[i].delai);
#endif
	i++;
    }

    fclose (fp);

    return (i);	    
}
