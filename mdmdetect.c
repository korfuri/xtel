/*	
 *   xtel - Emulateur MINITEL sous X11
 *
 *   Copyright (C) 1991-1998  Lectra Systemes & Pierre Ficheux
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
static char rcsid[] = "$Id: mdmdetect.c,v 1.3 2001/02/11 00:02:58 pierre Exp $";

/* Detection du modem pour generation du xtel.lignes */

#include "Config.tmpl"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef USE_TERMIOS
#include <termios.h>
#else
#include <termio.h>
#endif
#ifdef SVR4
#include <sys/mkdev.h>
#endif /* SVR4 */

#define TIMEOUT_READ		5
#define TEMPO			1000000

#ifndef HAS_STRERROR
extern char *sys_errlist[];
#define strerror(e) (sys_errlist[e])
#endif
extern char *xtel_basename (), *next_token ();

static int fdl;
static int fd;
static char timeout_reached;
static char *locked_line;
static char *progname;
static char *modem_list = MDM_LIST;
static char debug;
static char buf[256], lock_file[256];
static char *host;
static char *remote_list;
static char *builder;
static char query;

static void Usage (s)
char *s;
{
    fprintf (stderr, "Usage: %s [-d] [-l modem_list] [-b builder] [-q] tty_device\n", s);
    exit (1);
}

/* Sortie */
static void the_end (r)
int r;
{
  if (debug)
    log_debug ("the_end: %d", r);

  if (!query) {
    unlink (lock_file);
    restore_tty (fd);
    close (fd);
  }

  close (fdl);
  if (debug)
    close_debug ();
  exit (r);
}

static void sigint ()
{
  the_end (1);
}

/* Lock de ligne serie */
static void check_and_lock (char *line)
{
  char buf[256];
  struct stat statb;
  int fd, n;
  pid_t pid;

  /* A la mode UUCP... */
#ifdef SVR4
  sprintf (buf, "/dev/%s", line);
  if (stat (buf, &statb) != 0) {
      fprintf (stderr, "%s: %s\n", buf, strerror(errno));
      the_end (1);
  }

  sprintf (lock_file, FICHIER_LCK, (unsigned long) major(statb.st_dev), (unsigned long) major(statb.st_rdev), (unsigned long) minor(statb.st_rdev));
#else
  sprintf (lock_file, FICHIER_LCK, line);
#endif /* SVR4 */

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
			      fprintf (stderr, "Removed stale lock %s (pid %d)\n", lock_file, pid);
			      /* et on retente la création dudit fichier */
			      fd = -1;
			  }
			  else {
			      fprintf (stderr, "Can't remove stale lock %s\n", lock_file);
			      the_end (1);
			  }
		      }
		      else {
			  fprintf (stderr, "Device %s is already locked by pid %d\n", line, pid);
			  the_end (1);
		      }
		  }
		  else {
		      fprintf (stderr, "Can't read pid from lock file %s\n", lock_file);
		      the_end (1);
		  }
	      }
	      else if (errno != ENOENT) {
		  /* il ne vient pas d'être effacé par un autre programme */
		  /* c'est donc un vrai problème */
		  fprintf (stderr, "%s: %s\n", lock_file, strerror(errno));
		  the_end (1);
	      }
	  }
	  else {
	      /* fichier impossible à créer */
	      fprintf (stderr, "Can't create lock file %s (%s)\n", lock_file, strerror(errno));
	      the_end (1);
	  }
      }
  } while (fd < 0);

  /* Le lock est posé ; il faut y inscrire le pid de ce programme */
  sprintf (buf, "%10d\n", getpid());
  if (write (fd, buf, 11) != 11) {
      fprintf (stderr, "Error writing to file %s (%s)\n", lock_file, strerror(errno));
      close (fd);
      the_end (1);
  }
  close (fd);
}

/* Lecture d'une ligne terminee par '\n' */
static int read_a_line (int fd, char *buf, int size)
{
  char *p = buf, c;

  while (read (fd, &c, 1) > 0) {
    if (c == '\n' || ((p-buf) == size-1)) {
      if (c == '\n')
	*p++ = '\n';
      *p = 0;
      return 1;
    }

    *p++ = c;
  }

  return -1;
}

/* Creation d'une socket client, domaine AF_INET */
int create_inet_client_socket (host, port)
char *host;
int port;
{
  int sock;
  struct sockaddr_in addr;
  struct hostent *hp;

  /* Recupere l'adresse de l'hote cible */
  if (!(hp = gethostbyname (host))){
    perror ("gethostbyname");
    return (-1);
  }

  /* On cree la socket */
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    perror ("socket");
    return (-1);
  }

  /* Preparons l'adresse pour le connect */
  /* Le type d'adresse */
  addr.sin_family = AF_INET;

  /* Le port (recupere dans le getservbyname) */
  addr.sin_port = htons(port);

  /* L'adresse de l'hote cible */
  bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);

  /* On se connecte */
  if (connect (sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror ("connect");
    return (-1);
  }

  return (sock);
}

/* Recherche de mot-cle */
static char* check_for_kw (int fd, char *s)
{
  static char modem_string[4096], chat_script[256];

  fprintf (stderr, "."); fflush (stderr);

  sprintf (chat_script, "%s\r OK", s);
  if (do_chat (fd, chat_script, TIMEOUT_READ, TEMPO, NULL, modem_string, sizeof(modem_string)) != 0)
    return NULL;

  if (debug)
    log_debug ("reponse= %s", modem_string);

  return modem_string;
}


main (ac, av)
int ac;
char **av;
{
  char cmd[6];
  char *cp, *fab, *modem_string, *reponse;
  register int i;
  int found = 0;
#ifdef DEBUG_XTELD
  debug = 1;
#endif

  progname = xtel_basename(av[0]);

  if (ac < 2)
    Usage (progname);

  /* Lecture de la ligne de commande */
  while (--ac) {
    cp = *++av;
    if (*cp == '-' && *++cp) {
      switch(*cp) {
      case 'b' :
	if (!--ac)
	  Usage(progname);
	builder = *++av; break;

      case 'd' :
	++debug; break;

      case 'l' :
	if (!--ac)
	  Usage(progname);
	modem_list = *++av; break;

      case 'q' :
	query = 1; break;
		      
      default:
	Usage (progname);
      }
    }
    else {
      break;
    }
  }

  if (debug)
    init_debug (progname);

  signal (SIGINT, sigint);

  /* Lecture de la liste des modems */
  /* Test URL http:// */
  if (strncmp (modem_list, "http://", 6) == 0) {
    host = next_token (&modem_list[7], "/");
    remote_list = next_token (NULL, "\000");

    fprintf (stderr, "Appel de %s...", host); fflush (stderr);
    if ((fdl = create_inet_client_socket (host, 80)) < 0)
      exit (1);

    /* Demande le fichier */
    sprintf (buf, "GET /%s\n", remote_list);
    write (fdl, buf, strlen (buf));
    fprintf (stderr, "OK\n");
  }
  else {
    if ((fdl = open (modem_list, O_RDONLY)) < 0) {
      perror (modem_list);
      exit (1);
    }
  }

  if (query) {
    /* Lecture de la liste des modems */
    while (read_a_line (fdl, buf, sizeof(buf)) > 0) {

      if (debug)
	log_debug ("(%s)", buf);

      if (buf[0] == '#' || buf[0] == '\n')
	continue;

      /* Fabricant */
      if (buf[0] == '[') {
	fab = strdup (&buf[1]);
	fab[strlen(fab)-2] = 0;
	if (query)
	  printf ("%s\n", fab);

	continue;
      }
    }
  }

  else {
    /* Test et lock de la ligne */
    check_and_lock (cp);

    sprintf (buf, "/dev/%s", cp);
    if ((fd = open (buf, O_RDWR|O_NDELAY)) < 0) {
      perror (buf);
      exit (1);
    }

    /* Ligne en mode 'raw' */
    init_tty (fd, B9600, CS8, 2, 0, 0, TEMPO);

    /* Test de presence de modem */
    if (do_chat (fd, "AT\r OK", 3, TEMPO, NULL, NULL, 0)) {
      /* On insiste un peu */
      if (do_chat (fd, "AT\r OK", TEMPO, 3, NULL, NULL, 0)) {
	fprintf (stderr, "Pas de modem présent !\n");
	the_end (1);
      }
    }

    /* interrogation du modem sur l'ensemble des registres de configuration */
    strcpy (cmd, "ATI");
    for (i = -1 ; !found && i <= 9 ; i++) {
      if (i >= 0)
	sprintf (cmd, "ATI%d", i);
      if ((reponse = check_for_kw (fd, cmd)) != NULL) {
	/* on a une réponse : rechercher dans la base */
	lseek (fdl, 0, 0);
	found = 0;
	while (!found && read_a_line (fdl, buf, sizeof(buf)) > 0) {
	  if (debug)
	    log_debug ("(%s)", buf);
	  if (buf[0] == '#' || buf[0] == '\n') {
	    /* commentaire ou ligne blanche */
	  }
	  else if (buf[0] == '[') {
	    /* Fabricant */
	    fab = strdup (&buf[1]);
	    fab[strlen(fab)-2] = 0;
	  }
	  else if (!builder || strcmp (builder, fab)==0) {
	    /* description d'un modem */
	    modem_string = next_token (buf, ":");   /* Mot-clé à tester */
	    if (strstr (reponse, modem_string)) {
	      if (debug)
		log_debug ("%s trouve !", modem_string);
	      found = 1;
	    }
	  }
	}
      }
    }

    if (found) {
      if (debug)
	log_debug ("builder= %s modem id= %s", fab, modem_string);

      puts ("\n");

      /* Nom du modem */
      printf ("%s ", next_token (NULL, ":"));

      /* Separateur */
      printf ("%s ", next_token (NULL, ":"));
      
      /* Vitesse */
      printf ("%s ", next_token (NULL, ":"));
      
      /* Init */
      printf ("%s\n", next_token (NULL, "\n"));
      
      the_end (0);
    }
  }

  the_end (1);
}
