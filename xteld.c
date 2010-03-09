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
static char rcsid[] = "$Id: xteld.c,v 1.33 2001/02/13 09:40:49 pierre Exp $";

/*
 * Demon XTELD (communication avec le MODEM)
 */

/* 	  
 * Contributions:
 *
 *   Michel Fingerhut	IRCAM Paris
 *		 
 *	- Traitement du fichier de log
 *	- Acces proteges aux services
 *
 *   Pierre Beyssac	SYSECA
 *
 *	- traitement du Minitel 2
 *	- utilisation de syslog
 *
 *   Eric Delaunay
 *
 *	- support IAN
 *
 *   Bernard Fouche (CPIO)
 *
 *	- amelioration @pipe
 *
 */

#define EXTERN

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "demon.h"
#include "globald.h"
#include "patchlevel.h"

#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

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

#include <sys/socket.h>

#include <netinet/in.h>
#include <netdb.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif /* ndef MAXPATHLEN */

#define XTELD_INPUT	0
#define XTELD_OUTPUT	1

static int fin_fils;
static int pid_fils;
static int nb_services;
static fd_set a_lire;
static struct timeval timeout;
static time_t t_connexion;
static char buf[MAXPATHLEN], buf1[256], service[256], utilisateur[256];
static char flag_connexion;
static char parite;
static Boolean flag_serveur_local, flag_crlf;
static char type_client = CLIENT_XTEL;
static int current_page = 0;
static char erase_needed = 1;
static int tuyau_in[2], tuyau_out[2];
static int temps_maxi;
static int detecte_ian;
static int saisie_active;
static char fin_connexion = 0;
static struct definition_service services_hyper[MAX_SERVICES];
static int nb_services_hyper;
static char *peer_name;
static int first_col;
static Boolean iminitel_connected;
static char iminitel_script[MAXPATHLEN];

/* 
 * Table des caracteres accentues avec leurs equivalents ou approximations
 * en Videotex 
 */
static char *special_char_table[] = {
  "\031AA", "\031BA", "\031CA", "A", "\031HA", "\0310A", "A",
  "\031KC",
  "\031AE", "\031BE", "\031CE", "\031HE",
  "\031AI", "\031BI", "\031CI", "\031HI",
  "D", "N",
  "\031AO", "\031BO", "\031CO", "O", "\031HO",
  "X", "O",
  "\031AU", "\031BU", "\031CU", "\031HU",
  "Y", "P", "B",
  "\031Aa", "\031Ba", "\031Ca", "a", "\031Ha", "\0310a", "a",
  "\031Kc",
  "\031Ae", "\031Be", "\031Ce", "\031He",
  "\031Ai", "\031Bi", "\031Ci", "\031Hi",
  "d", "n",
  "\031Ao", "\031Bo", "\031Co", "o", "\031Ho",
  "-", "o",
  "\031Au", "\031Bu", "\031Cu", "\031Hu",
  "\031By", "p", "\031Hy"
};

static void display_control_line (char *);
static void erase_control_line (int);


static void sigsegv ()
{
#ifdef DEBUG_XTELD
  log_err ("Got SIGSEGV, attempting to dump core...");
#endif
  abort();         
}

static void demande_fin_fils()
{
#ifdef DEBUG_XTELD
    log_debug ("Demande la fin du fils");
#endif
    signal (SIGALRM, SIG_IGN);
    fin_fils = 1;
}


/*
 * Teste si un couple de processus xteld pere/fils existe
 * retourne 1 si oui.
 */
int check_existing_process ()
{
  DIR *dir;
  struct dirent *de;
  int pid, ok, ret = 0;
  char path[MAXPATHLEN], cmd[256];
  FILE *fp;

  if (!(dir = opendir (PROC_BASE)))
    return -1;

  while (de = readdir (dir)) {
    if (!(pid = atoi (de->d_name)) || pid == getpid() || pid == getppid()) 
      continue;

    /* Teste le nom du process */
    sprintf(path, PROC_BASE "/%d/stat", pid);
    if (!(fp = fopen (path,"r"))) 
      continue;
	
    ok = (fscanf (fp, "%*d (%[^)]", cmd) == 1);
    (void) fclose(fp);

    if (!strcmp ("xteld", cmd)) {
      ret = 1;
      break;
    }
  }

  (void) closedir(dir);  
  
  return ret;
}

/*
 * Lecture du fichier d'état I-minitel
 */
static int read_iminitel_file ()
{
  FILE *fp;
  struct stat statb;
  char buf[256];

  if (stat (IMINITEL_FILE, &statb) < 0) {
    if (errno != ENOENT) {
      /* Ne devrait pas arriver */
      erreur_a_xtel (IMINITEL_FILE, errno);
      exit (1);
    }
    else
      return 0;
  }

  /* Fichier non vide, on lit les parametres */
  if (statb.st_size > 0) {
    if (!(fp = fopen (IMINITEL_FILE, "r"))) {
      erreur_a_xtel (IMINITEL_FILE, errno);
      exit (1);
    }

    while (fgets (buf, sizeof(buf), fp) != NULL) {
      buf[strlen(buf)-1] = 0;

      if (buf[0] == 0)
	continue;

      putenv (strdup(buf));
    }

    fclose (fp);
    return 1;
  }
  else
    return 0;
}

static void iminitel_stop ()
{
  /*
   * Si dernière connexion I-Minitel, coupe la connexion PPP
   */
  if (iminitel_connected && !check_existing_process ()) {
    /* Execution du script de connexion avec option 'stop' */
    sprintf (buf, "%s/%s stop", XTEL_LIBDIR, iminitel_script);
#ifdef DEBUG_XTELD
    log_debug ("execute: %s", buf);
#endif
    if (system (buf) != 0) {
      erreur_a_xtel (buf, errno);
      exit (1);
    }
  }
}

/*
 * Transmet une erreur a XTEL 
 *	- chaine de caractere terminee par 0
 *	- errno associe
 *
 * En mode HyperTerminal, on affiche l'erreur sur la ligne 0
 */
void erreur_a_xtel (s, code_erreur)
char *s;
int code_erreur;
{
    char e = code_erreur;

    if (type_client == CLIENT_XTEL) {
      write (XTELD_OUTPUT, CHAINE_REPONSE_DEBUT_ERREUR, 1);
      write (XTELD_OUTPUT, s, strlen(s));
      write (XTELD_OUTPUT, "\0000", 1); 
      write (XTELD_OUTPUT, &e, 1);
      write (XTELD_OUTPUT, CHAINE_REPONSE_FIN_ERREUR, 1);
    }
    else if (type_client == CLIENT_W_HYPER) {
      if (e)
	sprintf (buf, "%s: %s", s, strerror(errno));
      else {
	if (*s == '[')
	  strcpy (buf, s + 3);
	else
	  strcpy (buf, s);
      }
		
      display_control_line (buf);
    }
}

static void usage (char *s)
{
  fprintf (stderr, "Usage: %s [-H] [-v]\n", s);
  exit (1);
}

/* 
 * Teste si l'utilisateur courant a acces au service demande
 *
 *	1 si oui
 *	0 sinon
 */
int
service_autorise (indice_service, maxtime)
int indice_service, *maxtime;
{    
     char *pt, *pt1, *auth = definition_services[indice_service].autorisations;
     char autorisations[2048], *host;
     int default_maxtime = 0;

     *maxtime = 0;

     /* Pas de protection ou definition general du temps maxi */
     if (*auth == 0)
       return 1;

     if (*auth == '=' && !strchr (auth, ':')) {
       *maxtime = atoi (auth+1);
       return 1;
     }

     strcpy (autorisations, auth);
     pt = strtok (autorisations, ":");
     while (pt != NULL) {
       /* 
	* Format: nom=temps_maxi (si =temp_maxi, le temps est la valeur 
	* par defaut)
	*/
       *maxtime = default_maxtime;

       if (*pt == '=') {
	 /* nouvelle valeur par defaut */
	 *maxtime = default_maxtime = atoi (pt+1);
       }
       else {
	 if ((pt1 = strchr (pt, '='))) {
	   *pt1 = 0;
	   *maxtime = atoi (pt1+1);
	 }

	 /* 
	  * Test d'autorisation 
	  * 
	  * Le nom peut etre sous la forme 'user@hostname' afin d'autoriser un 
	  * utilisateur donne d'une machine donnee (@hostname autorise tous les 
	  * utilisateurs de hostname).
	  *
	  * Si le hostname n'est pas present, tous les utilisateurs sont
	  * autorises (compatible avec les versions precedentes de xteld).
	  * Dans le cas de Windaube, le nom de l' utilisateur est ignore pour
	  * l'instant.
	  */

#ifdef DEBUG_XTELD
	 log_debug ("peer= %s user= %s pt= %s", peer_name, utilisateur, pt);
#endif
	 if (*pt == '@') {
	   if (!strcmp (pt+1, peer_name))
	     return 1;
	 }
	 else {
	   /* Sinon, test par user@host */
	   host = strchr (pt, '@');
	   if (host) {
	     *host = 0;
	     host++;
	     
	     if (!strcmp (host, peer_name) && (!strcmp (pt, utilisateur) || type_client != CLIENT_XTEL))
	       return 1;
	   }
	   else {
	     if (!strcmp (pt, utilisateur))
	       return 1;
	   }
	 }
       }

       pt = strtok(NULL, ":");
     }
     
     return(0);
}

/*
 * Fonction d'autorisation d'envoi de caractères au modem
 * (quand connexion restreinte à un service Teletel)
 */
static void active_saisie ()
{
#ifdef DEBUG_XTELD
    log_debug( "saisie autorisée" );
#endif
    saisie_active = 1;
    signal( SIGUSR1, SIG_DFL );
}

/*
 * Fonction de deconnexion (appelee sur SIGCHLD)
 */
static void waitchild ()
{
    int r;

    wait (&r);
}

static void deconnexion ()
{
    FILE *fplog;

    if (flag_connexion) {

#ifdef DEBUG_XTELD
	log_debug ("Deconnecte !");
#endif
	
	flag_connexion = 0;
	
	if (!flag_serveur_local) {

	    if (definition_lignes[numero_ligne].type_dialer != DIALER_MODEM) {

		/*
		 * Envoi du code de connexion/fin puis de la sequence
		 * de raccrochage
		 */
#ifdef DEBUG_XTELD
		log_debug ("Raccrochage Minitel");
#endif
		write (fd_modem, "\x13I\x1b\x39\x67", 5);
	    }

	    myundial (fd_modem);

	}
	else
	    kill (pid_fils, SIGTERM);
	
	/* signal a XTEL la deconnexion */
	write (XTELD_OUTPUT, CHAINE_REPONSE_DECONNEXION, 1);
	
#if 0 /* symlink attack vulnerability */
	/* supprime le fichier de log */
	sprintf (buf, "/tmp/.xtel-%s", utilisateur);
	unlink (buf);
#endif

	if ((fplog= fopen(FICHIER_LOG, "a")) != NULL) {
	    long t= time(0), duree;
	    char *at= ctime(&t);
	    at[24]= '\000';

	    duree = (t_connexion == 0 ? 0L : t-t_connexion);
	    fprintf(fplog, "%s, %s deconnexion de : %s (%ld s sur %s)\n", at, utilisateur, service, duree, definition_lignes[numero_ligne].nom);
	    fclose(fplog);
	}
	
	waitchild ();
	fin_connexion++;
    }
}

/* Test de la mort subite de Xtel en cours de connexion (errno != EINTR) */
static void test_mort_subite ()
{
#ifdef DEBUG_XTELD
      log_debug ("test_mort_subite: errno=%d", errno);
#endif
      /* Si l'erreur est bizarre et que l'on est toujours connecte, 
	 force la deconnexion */
      if (errno != EINTR && flag_connexion) {
	  kill (pid_fils, SIGTERM);
	  deconnexion ();
      }
}    

/* Test chaine = nombre */
static is_number (s)
char *s;
{
    register char *p = s;

    if (!s)
	return 0;

    while (*p) {
	if (!isdigit(*p))
	    return 0;

	p++;
    }

    return 1;
}

/* Separation de chaine */
static char *separe_chaine (s, c)
char *s;
char c;
{
    register char *p = s;
    char *s1;
    
    while (*p) {
	if (*p == c) {
	    s1 = strdup (p+1);
	    *p = 0;
	    return s1;
	}

	p++;
    }

    return NULL;
}

/* 
 * Trouve le device associe a un numero de telephone 
 * ou un nom de service 
 */
static char *find_device (service, index)
char *service;
int *index;
{
  register int i, idev;
  char *numero_direct, *pt1, *pt2;

  /* On precise le device "a la main" (ex: 3616,le_v23) */
  numero_direct = strchr (service, ',');
	
  /* Nom du device associe au service */
  if (numero_direct) {
    *numero_direct++ = 0;
    return (numero_direct);
  }
  else {
    for (i = 0 ; i != nb_services ; i++) {
      /* Si on n'a pas precise le device pour un appel direct, on utilise le
	 premier trouve */
      if (!strcmp (definition_services[i].nom_service, "Direct")) {
	  break;
      }
      else {
	/* Nouvelle interface: xtel envoit le nom du service et non plus le
	   numero de telephone. Cela permet d'utiliser plusieurs modems pour le
	   meme numero de telephone */
	if (*service == '\001') {
	  pt1 = definition_services[i].nom_service;
	  pt2 = service + 1;
	}
	else {
	  pt1 = definition_services[i].nom_uucp;
	  pt2 = service;
	}

#ifdef DEBUG_XTELD
	log_debug ("pt1= %s pt2= %s", pt1, pt2);
#endif

	if (!strcmp (pt1, pt2)) {
	  break;
	}
      }
    }

    /* Service non trouve */
    if (i == nb_services)
      return NULL;

    idev = i;

    (void)service_autorise (idev, &temps_maxi);
#ifdef DEBUG_XTELD
    log_debug ("find_device: idev= %d %s tmax %d", idev, definition_services[idev].device, temps_maxi);
#endif
    *index = idev;

    return (definition_services[idev].device);
  }
}

/* 
 * Connexion a un service 
 */
void appel_service (service_teletel)
char *service_teletel;
{
    char *device_associe = NULL, *option = NULL;
    char *code_teletel = NULL, *code_service = NULL;
    FILE *fplog;
    register int i;
    int idef;

#ifdef DEBUG_XTELD
    log_debug ("appel_service: service_teletel= %s", service_teletel);
#endif

    detecte_ian = 0;
    saisie_active = 0;

    signal (SIGTERM, demande_fin_fils);

    /* Calcul du device associe */
    device_associe = find_device (service_teletel, &idef);

    /* Pas de device ==> erreur */
    if (!device_associe) {
      erreur_a_xtel ("[2] Pas de device correspondant !", 0);
      return;
    }

    /* Nouvelle interface ? */
    if (*service_teletel == '\001') {
      if (idef >= 0)
	code_teletel = definition_services[idef].nom_uucp;
    }
    else
      code_teletel = service_teletel;

#ifdef DEBUG_XTELD
    log_debug ("appel_service: device_associe= [%s] service_teletel= %s code_teletel= %s utilisateur= %s", device_associe, service_teletel, code_teletel, utilisateur);
#endif

    /* Service local par "pipe" */
    if (strncmp (device_associe, "@pipe", 5) == 0) {
	flag_serveur_local = True;

	/* Options du service local (pour l'instant "crlf") */
	if (strcmp (&device_associe[5], ":crlf") == 0)
	    flag_crlf = True;

	if (pipe (tuyau_in) < 0) {
	    log_err ("pipe: tuyau_in: %m");
	    exit (1);
	}

	if (pipe (tuyau_out) < 0) {
	    log_err ("pipe: tuyau_out: %m");
	    exit (1);
	}

 	if (!fork()) { /* fiston */
	  char *p, *q, **Argv, End;
	  int i, Argc = 0;

#ifdef DEBUG_XTELD
	  log_debug ("le fiston = %d", getpid());
#endif
	  dup2 (tuyau_out[0], 0);
	  close (tuyau_out[0]);
	  dup2 (tuyau_in[1], 1);
	  close (tuyau_in[1]);

	  /* On recupere les options eventuelles */
	  p = q = code_teletel;
	  for (;;) {
	    if ((*p == ' ') || (!*p))
	      {
		End = *p;
		*p = 0;
		if (!Argc)
		  Argv = malloc (sizeof (char **));
		else
		  Argv = realloc (Argv, (sizeof(char *)*Argc+1));
		Argv[Argc++] = strdup (q);
		if (!End)
		  break;
		q = ++p;
	      }
	    else
	      p++;
	  }
	  Argv = realloc (Argv, (sizeof(char *)*Argc+1));
	  Argv[Argc] = NULL;

#ifdef DEBUG_XTELD
	  log_debug ("code_teletel %s", code_teletel);
	  for (i = 0 ; i <= Argc ; i++)
	    log_debug ("Argv[%d] = %s", i, Argv[i] ? Argv[i] : "NULL");
#endif

	  if (execvp (code_teletel, Argv) < 0) {
	    log_err ("execlp: %m");
	    exit (1);
	  }
	}
	else {
#ifdef DEBUG_XTELD
	    log_debug ("le papa = %d", getpid());
#endif
	    fd_modem = tuyau_in[0];
	}
    }
    else 
    /* 
     * Connexion TCP ou I-Minitel 3622 
     *
     * Pour I-Minitel le parametre est constitué du script de connexion et du port
     * ex: connect_iminitel.sh:7516
     */

      if (!strncmp (device_associe, "@tcp", 4) || !strncmp (device_associe, "@imi", 4)) {
	char *tcp_port;
	int maxtime;
	int fd;

	/* Valide la saisie */
	saisie_active = 1;
	
	/* Recupere le numero de port ou nom de service + le nom du serveur */
	tcp_port = separe_chaine (code_teletel, ':');

	/* En cas de connexion I-Minitel, on effectue la connexion PPP */
	if (!strncmp (device_associe, "@imi", 4)) {
	  strcpy (iminitel_script, code_teletel);

	  /* On vérifie si déjà connecté (i.e. existence du fichier lock
	   * /var/run/ppp-iminitel.pid qui contient le pid du processus ppp
	   * normalement encore actif) */
	  fd = open( IMINITEL_LOCKFILE, O_RDONLY );
	  if (fd >= 0) {
	      /* le fichier existe : voir si le programme qui l'a créé existe toujours */
	      char buf[20];
	      int n;
	      /* lit la première ligne contenant le PID du processus qui l'a créé */
	      n = read( fd, buf, sizeof(buf) );
	      close( fd );
	      if (n > 0) {
		  /* récupère le PID */
		  pid_t pid;
		  buf[n] = '\0';
		  pid = (pid_t)atoi( buf );
		  /* teste l'existence du processus ayant créé le fichier */
		  if (pid == 0 || (kill(pid,0) == -1 && errno == ESRCH)) {
		      if (unlink(IMINITEL_LOCKFILE) == 0) {
#ifdef DEBUG_XTELD
			  log_debug ("Removed stale lock %s (pid %d)", IMINITEL_LOCKFILE, pid);
#endif
			  /* et on redémarrera la connexion ppp */
			  fd = -1;
		      }
		      else {
			  erreur_a_xtel (IMINITEL_LOCKFILE, errno);
			  return;
		      }
		  }
		  else {
		      /* le processus existe encore -> connexion supposée active */
		  }
	      }
	  }
	  else if (errno != ENOENT) {
	      /* il ne vient pas d'être effacé par un autre programme */
	      /* c'est donc un vrai problème */
	      erreur_a_xtel (IMINITEL_LOCKFILE, errno);
	      return;
	  }
	  /* Si pas deja connecte, on lance la connexion */
	  if (fd < 0) {
	      /* Effacer éventuellement le fichier d'état I-Minitel */
	      if (unlink( IMINITEL_FILE ) < 0 && errno != ENOENT) {
		  erreur_a_xtel( IMINITEL_FILE, errno );
		  return;
	      }
	      /* Execution du script de connexion avec option 'start' */
	      sprintf (buf, "%s/%s start", XTEL_LIBDIR, iminitel_script);
#ifdef DEBUG_XTELD
	      log_debug ("execute: %s", buf);
#endif
	      if (system (buf) != 0) {
		  erreur_a_xtel (buf, errno);
		  exit (1);
	      }
	  }

	  /* Temps maxi d'attente de connexion */
	  maxtime = IMINITEL_TIMEOUT;

#ifdef DEBUG_XTELD
	  log_debug ("attente connexion: %d secondes", maxtime);
#endif

	  /* Attente connexion */
	  do {
	    if (read_iminitel_file () > 0)
	      break;

	    sleep (1);
	  } while (--maxtime);

	  /* Connexion */
	  if (!maxtime) {
	    erreur_a_xtel ("[3] Erreur connexion PPP (timeout) !", 0);
	    exit (1);
	  }
	  else { /* Connecté à I-Minitel */
#ifdef DEBUG_XTELD
	    log_debug ("Connecte à I-Minitel ! (%s)",  getenv ("IMINITEL_SERVER"));
#endif
	    iminitel_connected = True;
	    strcpy (buf, getenv ("IMINITEL_SERVER"));
	  }
	}
	else /* Copie simplement le nom du serveur */
	  strcpy (buf, code_teletel);

#ifdef DEBUG_XTELD
	log_debug ("tcp serveur= %s", buf);
#endif

	if (is_number (tcp_port))
	  fd_modem = c_clientbyport (buf, atoi(tcp_port));
	else
	  fd_modem = c_clientbyname (buf, tcp_port);

	/* Erreur de connexion TCP */
	if (fd_modem < 0) {
	  erreur_a_xtel (buf, errno);
	  iminitel_stop ();
	  exit (1);
	}
      }
    else { /* Connexion MODEM */
	/*
	 * sépare téléphone et code service (nnnn:code)
	 */
	char *telno = strdup (code_teletel);
	code_service = strchr (telno, ':');
	if (code_service) {
	    *code_service = 0;
	    code_service = strchr (code_teletel, ':') + 1;
	    detecte_ian = IAN_DE_GARDE;	/* attente capture code d'entrée sur le service */
	    ian_init (telno);
	    signal (SIGUSR1, active_saisie);
	}
	else
	    saisie_active = 1;

	/*
	 * compose le numéro
	 */
#ifdef DEBUG_XTELD
	log_debug( "service appele : %s [%s]", telno, code_service ? code_service:"" );
#endif
	if ((fd_modem = mydial (telno, device_associe)) < 0) {
#ifdef DEBUG_XTELD
	    log_debug ("meurt (erreur)");
#endif
	    exit (1);
	}

	free(telno);
    }

    /* valide le signal de deconnexion */
    signal (SIGCHLD, deconnexion);
    
    /* Init masque select */
    FD_ZERO (&a_lire);	
    FD_SET (fd_modem, &a_lire);
	
#ifdef DEBUG_XTELD
    log_debug ("Connecte !");
#endif
	
    flag_connexion = 1;	/* On est connecte ! */

    if (type_client == CLIENT_XTEL) {
      /* Envoi du temps maxi de connexion */
      if (temps_maxi) {
	char l;

	write (XTELD_OUTPUT, CHAINE_TEMPS_MAXI, 1);
	sprintf (buf, "%d", temps_maxi);
	l = strlen(buf);
	write (XTELD_OUTPUT, &l, 1);
	write (XTELD_OUTPUT, buf, l);
      }

      /* signale la connexion a XTEL */
      write (XTELD_OUTPUT, CHAINE_REPONSE_CONNEXION, 1);
    }
    else
      display_control_line ("CONNECTE AU SERVICE");

    /*
     * On cree un processus qui lit le modem et ecrit sur le reseau.
     * On tue ce processus au bout de DELAI_DECONNEXION secondes sans activite 
     * (ce qui fait raccrocher le MODEM)
     */
    if ((pid_fils = fork()) == 0) { /* fils */
	int etat = 0;
	int ignore = 0;
	int code_fin = 1;

	/* Duree maxi autorisee */
	if (!flag_serveur_local && temps_maxi > 0) {
          signal (SIGALRM, demande_fin_fils);
	  alarm (temps_maxi);
	}

	fin_fils = 0;
	while (!fin_fils) {
	    int nread, size, i;
	    fd_set t_a_lire;
	    
	    timeout.tv_sec = (unsigned long)DELAI_DECONNEXION;

	    t_a_lire = a_lire;
	    nread = select (32, &t_a_lire, NULL, NULL, (flag_serveur_local ? NULL : &timeout));

	    if (nread < 0 && errno == EINTR) {
		/* select() interrompu par le SIGTERM du parent */
#ifdef DEBUG_XTELD
		log_debug ("select() interrompu par le SIGTERM du parent");
#endif
		continue;
	    }
	    
	    if (nread == 0 || !FD_ISSET (fd_modem, &t_a_lire)) {
		/* read = 0 (timeout) ==> deconnexion */
#ifdef DEBUG_XTELD
		log_debug ("read = 0 (timeout) ==> deconnexion");
#endif
		code_fin = 2;
		break;
	    }
	    
	    size = read (fd_modem, buf, sizeof(buf));

	    if (size <= 0) {
#ifdef DEBUG_XTELD
		log_debug ("size <= 0");
#endif
		code_fin = 3;
		break;
	    }
	    
	    /*
	     * termine la connexion lors du retour à la page
	     * du service Teletel (detecte_ian == 1)
	     * ou autorise le service (detecte_ian == 2).
	     */
	    if (detecte_ian) {
		int ian;

		for( i = 0 ; i < size ; i++ )
		    if (ian = ian_valide (detecte_ian, buf[i] )) {
			switch (detecte_ian) {
			case IAN_DE_GARDE:
#ifdef DEBUG_XTELD
			    log_debug ("Emission code du service : %s + ENVOI", code_service );
#endif
			    /* ecrit le code du service */
			    write ((flag_serveur_local ? tuyau_out[1] : fd_modem),
				    code_service, strlen(code_service) );
			    /* + touche ENVOI */
			    write ((flag_serveur_local ? tuyau_out[1] : fd_modem),
				    "\023A", 2 );
			    detecte_ian = IAN_D_ENTREE;  /* autorise le service */
			    break;

			case IAN_D_ENTREE:
			    if (ian == 1) {
#ifdef DEBUG_XTELD
				log_debug( "service activé. Envoi SIGUSR1 pid %d", getppid() );
#endif
				detecte_ian = IAN_DE_FIN;
				/*
				 * autorise le père à transmettre les actions
				 * au fils, maintenant que le service a été validé
				 */
				kill( getppid(), SIGUSR1 );
			    }
			    else {
				/* devrait faire une nouvelle tentative ? */
#ifdef DEBUG_XTELD
				log_debug( "accès refusé" );
#endif
				fin_fils = 1;	/* échec */
			    }
			    break;

			case IAN_DE_FIN:
#ifdef DEBUG_XTELD
			    log_debug( "retour à la page Teletel" );
#endif
			    fin_fils = 1;     /* termine la connexion */
			}
			break;
		    }
	    }

	    for (i = 0; i < size; i++) {
		if (definition_lignes[numero_ligne].cs != CS8)
		    buf[i] &= 0x7f;
		
		if (definition_lignes[numero_ligne].type_dialer != DIALER_MODEM) {
		    /*
		     * Detection de la sequence SEP 53 signifiant
		     * la fin de la connexion
		     */
		    switch (etat) {
		      case 0:
			if (buf[i] == 0x13) {
			    /*
			     * Remplacer par des 0 toutes les sequences
			     * SEP xx venant du minitel pour eviter
			     * des echos parasites.
			     */
			    etat = 1;
			    ignore = 2;
			}
			break;

		      case 1:
			if (buf[i] == 0x53)
			    /* Sequence SEP 53 reconnue */
			    fin_fils = 1;
			etat = 0;
			break;
		    }
		    if (ignore) {
			ignore--;
		    }
		}
	    }

	    /* Conversion LF --> CF/LF si serveur local ? */
	    if (flag_serveur_local && flag_crlf) {
		register int i, size1;

		size1 = 0;
		for (i = 0 ; i != size ; i++) {
		    if (buf[i] == '\n') {
			buf1[size1++] = '\n';
			buf1[size1++] = '\r';
		    }
		    else
			buf1[size1++] = buf[i];
		}

		write (XTELD_OUTPUT, buf1, size1);
	    }
	    else
		write (XTELD_OUTPUT, buf, size);
	}

	/* Sortie du fils */
#ifdef DEBUG_XTELD
	log_debug ("meurt, code = %d", code_fin);
#endif
	iminitel_stop ();

	exit (0);
    }
    
    signal(SIGTERM, SIG_DFL);

    /* creation du fichier de log */
#if 0 /* symlink attack vulnerability */
    sprintf (buf, "/tmp/.xtel-%s", utilisateur);
    if ((fplog = fopen (buf, "w"))) {
#ifdef DEBUG_XTELD
	log_debug ("Creation du fichier de log %s", buf);
#endif
	chmod (buf, 0644);
	if (!flag_serveur_local) 
	    fprintf (fplog, "LIGNE = %s\n", definition_lignes[numero_ligne].nom);
	fprintf (fplog, "CLIENT = %s [%s]\n", peer_name, (type_client == CLIENT_XTEL ? "Xtel" : "HyperTerminal"));
	fprintf (fplog, "PROCESSUS = %d,%d\n", getpid(), pid_fils);
	fprintf (fplog, "SERVICE = %s\n", service);
	fclose (fplog);
    }
#endif
    
    if ((fplog= fopen(FICHIER_LOG, "a")) != NULL) {
	char *at;
	
	t_connexion = time(0);
	at = ctime(&t_connexion);
	at[24]= '\000';
	
	if (flag_serveur_local)
	    fprintf(fplog, "%s, %s appel de : %s (local)\n", at, utilisateur, service);
	else
	    fprintf(fplog, "%s, %s appel de : %s sur %s\n", at, utilisateur, service, definition_lignes[numero_ligne].nom);
	fclose(fplog);
    }
#ifdef NO_TERMIO
    parite = definition_lignes[numero_ligne].parity;
#endif
}

int lire_chaine(ch)
char *ch;
{
    int ret;
    unsigned char l;

    do {
	ret = read (XTELD_INPUT, &l, 1);
	if (ret <= 0) goto err;
    } while (ret != 1);

    ch[l] = 0;

    while (l) {
	ret = read (XTELD_INPUT, ch, l);
	if (ret <= 0) goto err;
	l -= ret;
    }

    return 0;

err:
    log_err ("read: %m");
    return -1;
}


static void trop_tard ()
{
    log_err ("Pas de reponse du client.\n");
    exit (1);
}

static char *get_peer_name ()
{
  struct sockaddr_in client;
  int len;

  /* Adresse du client */
  len = sizeof (client);
  if (getpeername (XTELD_INPUT, (struct sockaddr *) &client, &len) < 0) {
    log_err ("getpeername: %m");
  }
  else {
    struct hostent *h;

    if ((h = gethostbyaddr ((char *)&client.sin_addr, sizeof(client.sin_addr), AF_INET)) == NULL) {
      log_err ("gethostbyaddr: %m");
      return NULL;
    }
    else
	return (strdup (h->h_name));
  }
}

/* Traitement pour HyperTerminal */

/* 
 * Affiche une chaine ISO accentuee (conversion en Videotex)
 */
static void write_chaine_accentuee (s)
char *s;
{
    register unsigned char *p = s;
    char c;

    while (*p) {
      if (*p > 192) {
	int i = *p - 192;
	write (XTELD_OUTPUT, special_char_table[i], strlen(special_char_table[i]));
      }
      else
	write (XTELD_OUTPUT, p, 1);

      p++;
    }
}	

/* Repositionne le curseur a la position courante */
static void set_cursor (int index) 
{
  char str[256];

  sprintf (str, "\037U%c", first_col + index);
  write (XTELD_OUTPUT, str, strlen (str));
}

/* Affiche un message a la ligne 0 */
static void display_control_line (char *s)
{
  char str[256];

#ifdef DEBUG_XTELD
  log_debug ("display_control_line: %s", s);
#endif
  sprintf (str, "\024\037@A\030%s\021", s);
  write (XTELD_OUTPUT, str, strlen (str));
  erase_needed = 1;
}

/* Efface la ligne 0 */
static void erase_control_line (int index)
{
  char str[256];

#ifdef DEBUG_XTELD
  log_debug ("erase_control_line: erase_needed = %d", erase_needed);
#endif
  if (erase_needed) {
    strcpy (str, "\024\037@A\030\021");
    write (XTELD_OUTPUT, str, strlen (str));
    if (index >= 0)
      set_cursor (index);

    erase_needed = 0;
  }
}

/* Lecture des services autorises */
static void read_services_hyper ()
{
  register int i, j;

  j = 0;
  for (i = 0 ; i != nb_services ; i++) {
      if (service_autorise (i, &temps_maxi)) {
	services_hyper[j].nom_service = definition_services[i].nom_service;
	services_hyper[j].nom_uucp = definition_services[i].nom_uucp;
#ifdef DEBUG_XTELD
  log_debug ("%d %s %s", j, services_hyper[j].nom_service, services_hyper[j].nom_uucp);
#endif

	j++;
      }
  }

  nb_services_hyper = j;
#ifdef DEBUG_XTELD
  log_debug ("%d/%d services autorises", nb_services_hyper, nb_services);
#endif
}

/* Affiche un page de services disponibles */
static void display_page (int start)
{
  register int i, ii, istart;

  istart = start * 13;

  if (istart > nb_services_hyper)
    return;

  /* Efface la page precedente */
  for (ii = 0 ; ii != 13 ; ii++) {
    sprintf (buf, "\037%cB\033P\030", ii + 'F');
    write_chaine_accentuee (buf);
  }

  /* Affiche la nouvelle */
  for (ii = 0 ; ii != 13 ; ii++) {
    i = ii + istart;
    if (i < nb_services_hyper) {
      sprintf (buf, "\037%cB\033R\033D %2d\033F\033P %s\n", ii + 'F', i + 1, services_hyper[i].nom_service);
      write_chaine_accentuee (buf);
    }
    else
      break;
  }
}

/* Saisie dans l'ecran Videotex */
static void erase_input (char *str, int size)
{
  memset (str, 0, size);
  set_cursor (0);
  while (size--)
    write (XTELD_OUTPUT, ".", 1);
  set_cursor (0);
}

static int check_value (char *s, int maxv)
{
  if ((maxv && (atoi (s) > maxv)) || strlen (s) == 0) {
    display_control_line ("VALEUR INTERDITE !");
    return 0;
  }
  else
    return 1;
}

static int input_value (char *str, int max_value, int max_size, int numeric)
{
  char c;
  int ind = 0, end = 0;

  if (max_size > IN_SIZE)
    max_size = IN_SIZE;

  erase_input (str, max_size);
  
  do {
    if (read (XTELD_INPUT, &c, 1) <= 0)
      return -1;

#ifdef DEBUG_XTELD
    log_debug ("read ==> %x", c);
#endif

    erase_control_line (ind);

    /* Traitement ENVOI/SUITE/RETOUR.. */
    if (c == 19) {
      if (read (XTELD_INPUT, &c, 1) <= 0)
	return -1;

      switch (c) {

      case 'A' : /* ENVOI */
	if (!check_value (str, max_value)) {
	  erase_input (str, max_size);
	  ind = 0;
	}
	else
	  end = 1;

	break;

      case 'B' : /* RETOUR */
	if (current_page > 0) 
	  display_page (--current_page);
	else
	  display_control_line ("PREMIERE PAGE");
	set_cursor (ind);

	break;


      case 'H' : /* SUITE */
	if ((current_page  + 1) * 13 > nb_services)
	  display_control_line ("DERNIERE PAGE");
	else
	  display_page (++current_page);

	set_cursor (ind);

	break;

      case 'G' : /* CORRECTION */
	if (ind > 0) {
	  str[--ind] = 0;
	  set_cursor (ind);
	  write (XTELD_OUTPUT, ".", 1);
	  set_cursor (ind);
	}

	break;

      case 'E' : /* ANNULATION */

	erase_input (str, max_size);
	ind = 0;
	break;


      default :
	break;
      }
    }
    else if (c == '\r') {
      if (!check_value (str, max_value)) {
	erase_input (str, max_size);
	ind = 0;
      }
      else
	end = 1;
    }
    else {
      if (ind < max_size && (!numeric || (numeric && (c >= '0' && c <= '9')))) {
	str[ind++] = c;
	write (XTELD_OUTPUT, &c, 1);
#ifdef DEBUG_XTELD
	log_debug ("str = \"%s\"", str);
#endif
      }
    }
  } while (!end);

  return (strlen (str));
}

/* Init du mode HyperTerminal */
int lire_initial_hyper (ch)
char *ch;
{
  int ret, iserv, ind = 0;
  char c, str_in[IN_SIZE+1];

#ifdef DEBUG_XTELD
  log_debug ("lire_initial_hyper");
#endif

  memset ((void*)str_in, 0, sizeof(str_in));
  current_page = 0;
  
  /* Nom du PC connecte */
  if (peer_name)
    strcpy (ch , peer_name);
  else
    strcpy (ch, "hyper");

#ifdef DEBUG_XTELD
  log_debug ("lire_initial_hyper, utilisateur= %s", ch);
#endif

  first_col = 'O';

  /* Envoi des codes Videotex de la page */
  sprintf (buf, INIT1_HYPER, version_xtel, revision_xtel, PATCHLEVEL);
  write (XTELD_OUTPUT, buf, strlen(buf));

  /* Lecture et affichage des services disponibles */
  read_services_hyper ();
  display_page (current_page);

  write (XTELD_OUTPUT, INIT2_HYPER, strlen(INIT2_HYPER));
  erase_control_line (ind);
    
  /* Saisie utilisateur */
  if (input_value (str_in, nb_services_hyper, 2, True) < 0)
    goto err;

  iserv = atoi(str_in)-1;

  /* Appel direct, on saisit le numero de telephone */
  if (!strcmp (services_hyper[iserv].nom_service, "Direct")) {
    write (XTELD_OUTPUT, INIT3_HYPER, strlen(INIT3_HYPER));
    first_col = 'J';
    if (input_value (str_in, 0, 20, False) < 0)
      goto err;
    strcpy (service, str_in);
    sprintf (buf, "APPEL DIRECT %s", service);
  }

  /* Appel normal */
  else {
    sprintf (service, "\001%s", services_hyper[iserv].nom_service);
    sprintf (buf, "APPEL %s", services_hyper[iserv].nom_uucp);
  }

  display_control_line (buf);
  return 1;

err:
  log_err ("read: %m");

  return -1;
}



/*
 *	Partie principale 
 */
main (ac, av)
int ac;
char **av;
{
  unsigned char c;
  char *cp, *progname;
  register int i;
  int indice_service = 0;

  progname = xtel_basename (av[0]);
  init_debug (progname);

  signal (SIGSEGV, sigsegv);

    /* Lecture de la ligne de commande */
  while (--ac) {
    if ((cp = *++av) == NULL)
      break;
    if (*cp == '-' && *++cp) {
      switch(*cp) {

      case 'H' :
#ifdef DEBUG_XTELD
	log_debug ("Option Windaube/HyperTerminal active !");
#endif
	type_client = CLIENT_W_HYPER;
	break;
		      
      case 'v' : /* Version */

	fprintf (stderr, "%s for xtel version %d.%d.%d\n", progname, version_xtel, revision_xtel, PATCHLEVEL);
	exit (0);
	    
      default:
	usage (progname);
	break;
      }
    }
    else {
      break;
    }
  }

  if ((nb_lignes = lecture_configuration_lignes ()) < 0) {
    exit (1);
  }

  if ((nb_services = lecture_services ()) < 0) {
    exit (1);
  }


  /* Lecture du nom du client connecte */
  peer_name = get_peer_name ();

#ifdef DEBUG_XTELD
  log_debug ("Connexion XTEL...");
#endif

  /* Lecture du nom d'utilisateur */
  switch (type_client) {

  case CLIENT_W_HYPER:
    if (lire_initial_hyper (utilisateur) < 0)
      goto fin_xteld;

    appel_service (service);
    break;

  case CLIENT_XTEL :
    if (lire_chaine (utilisateur) < 0)
      goto fin_xteld;
		
    break;

  default :
    goto fin_xteld;
    break;
  }

  /* lecture connexion XTEL */
  while (!fin_connexion) {
    if (read (XTELD_INPUT, &c, 1) <= 0) {
      if (errno == EINTR)
	continue;
      else
	break;
    }

#ifdef DEBUG_XTELD
    if (c >= ' ')
      log_debug("Recu de xtel: `%c' (%02x)", c, c & 255);
    else
      log_debug("Recu de xtel: %02x", c & 255);
#endif


    if (flag_connexion) {
      if (c == VALEUR_COMMANDE_FIN) {
#ifdef DEBUG_XTELD
	log_debug ("tue le fils");
#endif		      
	kill (pid_fils, SIGTERM);
      }
      else {
#ifdef NO_TERMIO
	if (parite != SANS) {
	  register int p;
	  /* Calcul de parite... */			
	  p = (c & 0x0f) ^ (c >> 4);
	  p = (p & 3) ^ (p >> 2);      
	  p = (p & 1) ^ (p >> 1);
	  if (parite == IMPAIR)
	    p = ~p;
	  c = (c & 0x7f) | (p << 7);
	}
#endif /* NO_TERMIO */
	/*
	 * on écrit sur le modem que lorsque le code de service
	 * a été entré où qu'il n'y en a pas.
	 */
	if (flag_serveur_local)
	  write ( tuyau_out[1], &c, 1 );
	else
	  if (saisie_active) {
#ifdef DEBUG_XTELD
	    if (c >= ' ')
	      log_debug("Emission PAD: `%c' (%02x)", c, c & 255);
	    else
	      log_debug("Emission PAD: %02x", c & 255);
#endif
	    write ( fd_modem, &c, 1);
	  }
#ifdef DEBUG_XTELD
	  else
	    log_debug( "saisie perdue : `%c' (%d)", c, c );
#endif
      }
    }
    else {
      switch (c) {
			
      case VALEUR_COMMANDE_CONNEXION_M1 :

	appel_service (NULL);
	break;

      case VALEUR_COMMANDE_DEMANDE_CONNEXION :
			  
	/* lecture du service */
	if (lire_chaine(service) < 0)
	  fin_connexion++;
	else
	  /* connexion */
	  appel_service (service);
			  
	break;
			  
      case VALEUR_COMMANDE_SERVICE_SUIVANT :
			    
	if ((indice_service == nb_services) || flag_m1)
	  write (XTELD_OUTPUT, CHAINE_REPONSE_PLUS_DE_SERVICE, 1);
	else {
	  if (service_autorise (indice_service, &temps_maxi)) {
	    c = strlen (definition_services[indice_service].nom_service);
	    write (XTELD_OUTPUT, &c, 1);
	    write (XTELD_OUTPUT, definition_services[indice_service].nom_service, c);
#ifdef DEBUG_XTELD
	    log_debug ("service: %s", definition_services[indice_service].nom_service);
#endif		      
	  }
	  else { /* service interdit */
	    c = 0;
	    write (XTELD_OUTPUT, &c, 1);
	    indice_service++;
	  }
	}
			  
	break;
			  
      case VALEUR_COMMANDE_NOM_UUCP :
			    
	c = strlen (definition_services[indice_service].nom_uucp);
	write (XTELD_OUTPUT, &c, 1);
	write (XTELD_OUTPUT, definition_services[indice_service].nom_uucp, c);
#ifdef DEBUG_XTELD
	log_debug ("%s", definition_services[indice_service].nom_uucp);
#endif		      
	indice_service++;
			  
	break;
			  
      default :
			      
	break;
      }
    }
  }

 fin_xteld:
  close_debug ();
}
