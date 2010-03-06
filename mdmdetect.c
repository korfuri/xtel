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
#include <termio.h>
#ifdef SVR4
#include <sys/mkdev.h>
#endif /* SVR4 */

#define TIMEOUT_READ		5

#ifndef __FreeBSD__
#ifndef __GLIBC__
extern char *sys_errlist[];
#endif
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
  FILE *fplock;

  /* A la mode UUCP... */
#ifdef SVR4
  sprintf (buf, "/dev/%s", line);
  if (stat (buf, &statb) != 0) {
      fprintf (stderr, "%s: %s\n", buf, sys_errlist[errno]);
      the_end (1);
  }

  sprintf (lock_file, FICHIER_LCK, (unsigned long) major(statb.st_dev), (unsigned long) major(statb.st_rdev), (unsigned long) minor(statb.st_rdev));
#else
  sprintf (lock_file, FICHIER_LCK, line);
#endif /* SVR4 */

  /* Test du lock */
  if (stat (lock_file, &statb) == 0) {
    fprintf (stderr, "Lock file %s already exists, exiting.\n", lock_file);
    the_end (1);
  }

  /* Pose le lock */
  if (!(fplock = fopen (lock_file, "w"))) {
      fprintf (stderr, "%s: %s\n", lock_file, sys_errlist[errno]);
      the_end (1);
  }
  
  fprintf (fplock, "%10d\n", getpid());
  fclose (fplock);
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
static int check_for_kw (int fd, char *builder, char *kw, char *s)
{
  char modem_string[4096], chat_script[256];

  fprintf (stderr, "."); fflush (stderr);

  sprintf (chat_script, "%s\r OK", s);
  if (do_chat (fd, chat_script, TIMEOUT_READ, NULL, modem_string, sizeof(modem_string)) != 0)
    return 0;

  if (debug)
    log_debug ("[%s] check_for_kw %s: reponse= %s", builder, kw, modem_string);

  if (strstr (modem_string, kw)) {
      if (debug)
	log_debug ("%s trouve !", kw);

      return 1;
  }
      
  return 0;
}


main (ac, av)
int ac;
char **av;
{
  char *cp, *str1, *str2;
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
    if ((cp = *++av) == NULL)
      break;
    if (*cp == '-' && *++cp) {
      switch(*cp) {
      case 'b' :
	builder = *++av; break;

      case 'd' :
	++debug; break;

      case 'l' :
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

  if (!query) {
    /* Test et lock de la ligne */
    check_and_lock (cp);

    sprintf (buf, "/dev/%s", cp);
    if ((fd = open (buf, O_RDWR|O_NDELAY)) < 0) {
      perror (buf);
      exit (1);
    }

    /* Ligne en mode 'raw' */
    init_tty (fd, B9600, CS8, 2, 0, 0);

    /* Test de presence de modem */
    if (do_chat (fd, "AT\r OK", 3, NULL, NULL, 0)) {
      /* On insiste un peu */
      if (do_chat (fd, "AT\r OK", 3, NULL, NULL, 0)) {
	fprintf (stderr, "Pas de modem présent !\n");
	the_end (1);
      }
    }
  }

  /* Lecture de la liste des modems */
  while (read_a_line (fdl, buf, sizeof(buf)) > 0) {

    if (debug)
      log_debug ("(%s)", buf);

    if (buf[0] == '#' || buf[0] == '\n')
      continue;

    /* Fabricant */
    if (buf[0] == '[') {
      str1 = strdup (&buf[1]);
      str1[strlen(str1)-2] = 0;
      if (query)
	printf ("%s\n", str1);

      continue;
    }

    /* Si on a specifie un fabricant, teste le nom */
    if (query || (builder && strcmp (str1, builder)))
      continue;
      
    /* Mot-cle a tester */
    str2 = next_token (buf, ":");

    if (!str2) {
      fprintf (stderr, "Erreur de lecture %s\n", modem_list);
      the_end (1);
    }

    /* Interrogation par ATI puis ATIx */
    found = 0;
    if ((found = check_for_kw (fd, str1, str2, "ATI")) == 0) {
      for (i = '0' ; i <= '9' ; i++) {
	char cmd[6];

	sprintf (cmd, "ATI%c", i);
	if ((found = check_for_kw (fd, str1, str2, cmd)) > 0) {
	  break;
	}
      }
    }

    if (found) {
      if (debug)
	log_debug ("str2= %s buf= %s", str2, buf);

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
