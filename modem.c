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
static char rcsid[] = "$Id: modem.c,v 1.2 2001/02/13 09:39:23 pierre Exp $";

/* Init du modem */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef NO_TERMIO
#include <sgtty.h>
#else
#ifdef USE_TERMIOS
#include <sys/ioctl.h>
#include <termios.h>
#else
#include <termio.h>
#endif /* USE_TERMIOS */
#endif
#if (defined(lectra) && defined(SVR4) && !defined(sun)) || defined(hpux)
#include <sys/termiox.h>
#endif
#ifdef USE_SYSLOG
#include <syslog.h>
#endif /* USE_SYSLOG */

#include "demon.h"
#include "globald.h"

#ifdef NO_TERMIO
static struct sgttyb term_sauve, term;
#else
#ifdef USE_TERMIOS
static struct termios term_sauve, term;
#else
static struct termio term_sauve, term;
#endif /* USE_TERMIOS */
#endif /* NO_TERMIO */
#if (defined(lectra) && defined(SVR4) && !defined(sun)) || defined(hpux)
static struct termiox termx;
#endif /* lectra && SVR4 && !sun || hpux */

#ifndef SYSLOG
static FILE *fp_console;
#endif
static char prefix[256];
static struct timeval delai_maxi;

void init_debug (char *s)
{
  strcpy (prefix, s);
#ifdef USE_SYSLOG
    openlog(s, LOG_PID | LOG_CONS, LOG_INFO);
#else
    if ((fp_console = fopen ("/dev/console", "w")) == NULL) {
        perror ("/dev/console");
        exit (1);
    }
#endif /* USE_SYSLOG */
}

void close_debug ()
{
#ifndef USE_SYSLOG
  fclose (fp_console);
#endif
}

/* Syslog or not syslog ? */
#ifdef USE_SYSLOG
void log_debug (fmt, p1, p2, p3, p4, p5, p6, p7)
char *fmt;
int  p1, p2, p3, p4, p5, p6, p7;
{
    char msg[256];

    sprintf (msg, fmt, p1, p2, p3, p4, p5, p6, p7);
    syslog (LOG_INFO, msg);
}

void log_err (s)
char *s;
{
    syslog (LOG_ERR, s);
}
#else
void log_debug (fmt, p1, p2, p3, p4, p5, p6, p7)
char *fmt;
int  p1, p2, p3, p4, p5, p6, p7;
{
    fprintf (fp_console, "%s[%d] ", prefix, getpid());
    fprintf (fp_console, fmt, p1, p2, p3, p4, p5, p6, p7);
    fprintf (fp_console, "\n\r");
}

void log_err (s)
char *s;
{
    fprintf (fp_console, "%s[%d] ", prefix, getpid());

    while (*s) {
      if (*s != '%')
	fputc (*s, fp_console);
      else if (*(s+1) == 'm') {
	fprintf (fp_console, "%s", sys_errlist[errno]);
	s++;
      }

      s++;
    }

    fprintf (fp_console, "\n\r");
}

#endif /* USE_SYSLOG */


/*  Envoi d'une commande MODEM */
static void writemodem (fd, buf, n)
int fd;
char *buf;
int n;
{
    register int i;

    for (i = 0 ; i != n ; i++) {
        write (fd, buf+i, 1);
	usleep (30000);
    }
}

/* Init des parametres de ligne */
void init_tty (int fd, int speed, int csize, int parity, int flags, int dialer)
{
#ifdef NO_TERMIO
  ioctl (fd, TIOCGETP, &term);
  memcpy ((char *)&term_sauve, (char *)&term, sizeof(struct sgttyb));
  term.sg_flags |= RAW;
  ioctl (fd, TIOCSETP, &term);
	
  /* Flags, pour l'instant RTS/CTS */
  /* FIXME: comment passer la ligne en RTS/CTS sans termio ? */

#else

#ifdef USE_TERMIOS
  ioctl (fd, TIOCGETA, &term);
  memcpy ((char *)&term_sauve, (char *)&term, sizeof(struct termios));
#else
  ioctl (fd, TCGETA, &term);
  memcpy ((char *)&term_sauve, (char *)&term, sizeof(struct termio));
#endif /* USE_TERMIOS */
	
  /* Parametrage de la ligne */
  term.c_cc[VMIN] = 1;
  term.c_cc[VTIME] = 0;
  term.c_iflag &= ~(IXON|IXOFF|ICRNL);
  term.c_lflag &= ~(ICANON|ISIG|ECHO|IEXTEN);

#ifdef USE_TERMIOS
  /*
   * FreeBSD 1.1 (Beta) n'a pas l'air d'apprecier qu'on mette clocal
   * a 0... (blocage au premier write sur /dev/cua01)
   */
  term.c_cflag &= ~(CSIZE|CSTOPB);
  term.c_cflag |= (CREAD|HUPCL);
  term.c_ispeed = term.c_ospeed = speed;
#else
  term.c_cflag &= ~(CSIZE|CBAUD|CLOCAL);

  /* Vitesse */
  term.c_cflag |= speed;
#endif /* USE_TERMIOS */
  /* Taille caractere */
  term.c_cflag |= csize;

  /* Parite */
  if (parity == SANS)
    term.c_cflag &= ~PARENB;
  else {
    term.c_cflag |= PARENB;
    if (parity == PAIR) 
      term.c_cflag &= ~PARODD;
  }

  /* Flags, pour l'instant RTS/CTS */
  if (flags & FLAG_RTS_CTS) {
#ifndef __USLC__
#if !(defined(hpux) || defined(sgi) || (defined(lectra) && defined(SVR4) && !defined(sun)))
    term.c_cflag |= CRTSCTS;
#else
    ioctl (fd, TCGETX, &termx);
    termx.x_hflag |= (RTSXOFF|CTSXON);
    ioctl(fd, TCSETXW, &termx);
#endif /* HP/UX | SGI | LS/UX */
#endif /* !__USLC__ */
  }

#ifdef sun
  if (csize != CS8)
    term.c_iflag = ISTRIP;
#endif /* sun */

#ifdef USE_TERMIOS
  term.c_iflag &= ~(IGNCR|ICRNL|INLCR|IMAXBEL);
  term.c_iflag |= (ISTRIP|INPCK);
  term.c_lflag &= ~(ECHOCTL|IEXTEN);
  term.c_oflag &= ~OPOST;
#endif /* USE_TERMIOS */

  /* Cas du Minitel 1/2 */
  if (dialer != DIALER_MODEM)
    term.c_cflag |= (CREAD|CLOCAL);

  /* Affectation des parametres */
#ifdef USE_TERMIOS
  ioctl (fd, TIOCSETA, &term);
#else
  ioctl (fd, TCSETA, &term);
#endif /* USE_TERMIOS */
#endif /* NO_TERMIO */
}

/* Restauration des parametres */
void restore_tty (fd)
{
#ifdef ultrix
  int temp = 0;
#endif
 
   /* remet la ligne en l'etat */
#ifdef NO_TERMIO
  term.sg_ispeed = term.sg_ospeed = B0;
  ioctl (fd, TIOCSETP, &term);
  ioctl (fd, TIOCSETP, &term_sauve);
#else
#ifdef USE_TERMIOS
  term.c_ispeed = B0;
  term.c_ospeed = B0;
  ioctl (fd, TIOCSETAW, &term);
  ioctl (fd, TIOCSETA, &term_sauve);
#else
  term.c_cflag &= ~CBAUD;
  term.c_cflag |= B0;
  ioctl (fd, TCSETAW, &term);
  ioctl (fd, TCSETA, &term_sauve);
#endif /* USE_TERMIOS */
#endif /* NO_TERMIO */
#ifdef ultrix
  ioctl (fd, TIOCNMODEM, &temp);
  ioctl (fd, TIOCSINUSE, NULL);
  ioctl (fd, TIOCHPCL, 0);
#endif /* ultrix */
}

/* 
 * Dialogue avec le Modem (chatons, chatons...)
 */
int do_chat (fd, chat_script, tmax, telno, reply_buf, reply_size)
int fd;
char *chat_script;
unsigned long tmax;
char *telno, *reply_buf;
int reply_size;
{
    fd_set a_lire, t_a_lire;
    int i, erreur, fin, nbread, cmodem;
    char *pt_chat, c, *q;

    delai_maxi.tv_sec = tmax;
    erreur = 0;
    pt_chat = chat_script;
    fin = 0;
    FD_ZERO (&a_lire);
    FD_SET (fd, &a_lire);
    q = reply_buf;

#ifdef DEBUG_XTELD
    log_debug ("do_chat: chat= \"%s\" tmax = %ld", chat_script, tmax);
#endif

#ifndef NO_TERMIO
    /* 
     * Passe en controle local si necessaire. Cela permet d'eviter que le
     *  script se plante lorsque certains modem font bouger le DCD sur un AT&F
     *  (comme le Hayes Optima par exemple)...
     */
#ifdef USE_TERMIOS
    ioctl (fd, TCIOCGETA, &term);
#else
    ioctl (fd, TCGETA, &term);
#endif /* USE_TERMIOS */
    if ((term.c_cflag | CLOCAL) == 0) {
	term.c_cflag |= CLOCAL;
#ifdef USE_TERMIOS
	ioctl (fd, TCIOCSETA, &term);
#else
	ioctl (fd, TCSETA, &term);
#endif /* USE_TERMIOS */
	cmodem = 1;
    }
#endif /* NO_TERMIO */

    while (!fin) {
	/* 
	 * On commence par emettre la commande au Modem... (les chaines sont
	 * separees par des blancs). On attend ensuite la reponse.
	 */
#ifdef DEBUG_XTELD
      {
	char ci, *p; 
	int fi;

	p = strdup (pt_chat);
	for (i = 0 ; *(p+i) != ' ' && *(p+i) != 0 ; i++)
	  ;
	ci = p[i]; 
	p[i] = 0;
	log_debug ("J'envoie: \"%s\"", p);
	p[i] = ci;

	if (ci) {
	    i++; fi = i;
	    for ( ; *(p+i) != ' ' && *(p+i) != 0 ; i++);
	    ci = p[i]; 
	    p[i] = 0;
	    log_debug ("J'attends: \"%s\"", p + fi);
	    p[i] = ci;
	}

	free (p);
      }
#endif

	while (*pt_chat && *pt_chat != ' ') {
	    /*
	     * On reconnait les sequences '\' suivantes :
	     *
	     *	\T		numero de telephone
	     *	\d		tempo de 1 s
	     *	\n		LF
	     *	\r		CR
	     *	\t		VT
	     *	\a		BELL
	     *	\abc		valeur DECIMALE entre 0 et 255 sur 3 caracteres
	     *			(ex: \010 pour envoyer 10)
	     *	\xab		valeur hexadecimale
	     */

	    if (*pt_chat != '\\') {
#ifdef DEBUG_CHAT
		log_debug ("envoie: '%c'", *pt_chat);
#endif
		writemodem (fd, pt_chat, 1);
	    } else {
		pt_chat++;
		switch (*pt_chat) {
		  case 'd' :
		    sleep (1);
		    break;
		  case 'n' :
#ifdef DEBUG_CHAT
		    log_debug ("envoie: '\\n'");
#endif
		    writemodem (fd, "\n", 1);
		    break;
		  case 'r' :
#ifdef DEBUG_CHAT
		    log_debug ("envoie: '\\r'");
#endif
		    writemodem (fd, "\r", 1);
		    break;
		  case 't' :
#ifdef DEBUG_CHAT
		    log_debug ("envoie: '\\t'");
#endif
		    writemodem (fd, "\t", 1);
		    break;
		  case 'a' :
#ifdef DEBUG_CHAT
		    log_debug ("envoie: '\\a'");
#endif
		    writemodem (fd, "\a", 1);
		    break;
		  case 'T' :
		    if (telno) {
#ifdef DEBUG_CHAT
		      log_debug ("envoie: \"%s\"", telno);
#endif
		      writemodem (fd, telno, strlen(telno));
		    }

		    break;

		  case '0': case '1': case '2': case '3': case '4':
		  case '5': case '6': case '7': case '8': case '9': {
		      char nb = 2;
		      char n = *pt_chat++ - '0';
		      while (nb-- && isdigit(*pt_chat)) {
			  n *= 10;
			  n += *pt_chat++ - '0';
		      }
		      pt_chat--;
#ifdef DEBUG_CHAT
		      log_debug ("envoie: '\\0x%x'", n);
#endif
		      writemodem (fd, &n, 1);
		      break;
		  }

		  case 'x': {
		      char n = 0, nb = 2;
		      pt_chat++;
		      while (nb-- && isxdigit(*pt_chat)) {
			  n *= 16;
			  if (isdigit(*pt_chat)) {
			      n += *pt_chat - '0';
			  } else {
			      if (islower(*pt_chat)) {
				  n += *pt_chat + (10 - 'a');
			      } else {
				  n += *pt_chat + (10 - 'A');
			      }
			  }
			  pt_chat++;
		      }
		      pt_chat--;
#ifdef DEBUG_CHAT
		      log_debug ("envoie: '\\0x%x'", n);
#endif
		      writemodem (fd, &n, 1);
		      break;
		  }
		  default :
#ifdef DEBUG_CHAT
		    log_debug ("envoie: '%c'", *pt_chat);
#endif
		    writemodem(fd, pt_chat, 1);
		    break;
		}
	    }
	    pt_chat++;
	}
	if (*pt_chat == 0)
	    break;
	/*
	 * On attend la reponse.
	 */

	/* pointe la chaine a recevoir */
	pt_chat++;
	i = 0;

	for (;;) {
	    
	    t_a_lire = a_lire;
	    nbread = select (32, &t_a_lire, NULL, NULL, &delai_maxi);

	    /* Si il y a qque chose a lire */
	    if (nbread > 0 && FD_ISSET (fd, &t_a_lire)) {
		if (read (fd, &c, 1) == 1)  {
		    char voulu;

		    if (q && ((q - reply_buf) < reply_size-1))
		      *q++ = c;

#ifdef DEBUG_CHAT
		    log_debug ("recu: '%c'", c);
#endif
		    if (pt_chat[i] != '\\') {
			voulu = pt_chat[i];
		    } else {
			i++;
			switch(pt_chat[i]) {
			  case 'n':
			    voulu = '\n';
			    break;
			  case 'r':
			    voulu = '\r';
			    break;
			  case 't':
			    voulu = '\t';
			    break;
			  case 'a':
			    voulu = '\a';
			    break;
			  case '0': case '1': case '2':
			  case '3': case '4': case '5':
			  case '6': case '7': case '8':
			  case '9': {
			      char nb = 2;
			      voulu = pt_chat[i++] - '0';
			      while (nb-- && isdigit(pt_chat[i])) {
				  voulu *= 10;
				  voulu += pt_chat[i++] - '0';
			      }
			      i--;
			      break;
			  }
			  case 'x': {
			      char nb = 2;
			      voulu = 0;
			      i++;
			      while (nb-- && isxdigit(pt_chat[i])) {
				  voulu *= 16;
				  if (isdigit(pt_chat[i])) {
				      voulu += pt_chat[i] - '0';
				  } else {
				      if (islower(pt_chat[i])) {
					  voulu += pt_chat[i] + (10 - 'a');
				      } else {
					  voulu += pt_chat[i] + (10 - 'A');
				      }
				  }
				  i++;
			      }
			      i--;
			      break;
			  }
			  default:
			    voulu = pt_chat[i];
			    break;
			}
		    }
		    if (voulu == c) {
			i++;
#ifdef DEBUG_CHAT
			log_debug ("voulu '%c':Ok", voulu);
#endif
			if (*(pt_chat+i) == ' ' || *(pt_chat+i) == 0) {
#ifdef DEBUG_XTELD
			    log_debug (" -Ok");
#endif
			    break;
			}
		    }
		    else {
#ifdef DEBUG_CHAT
			log_debug ("voulu '%c':erreur", voulu);
#endif
			i = 0;
		    }
		}
		/* erreur read */
		else {
#ifdef DEBUG_XTELD
		    log_debug ("Erreur read !");
#endif
		    erreur = 1;
		    break;
		}
	    }
	    /* timeout */
	    else if (nbread == 0) {
#ifdef DEBUG_XTELD
		log_debug ("Erreur timeout !");
#endif
		erreur = 1;
		break;
	    }
	}

	/* Commande suivante */
	pt_chat += (i+1);
	fin = ((erreur != 0 || *(pt_chat-1) == 0) ? 1 : 0);
	usleep (30000);
    }

#ifndef NO_TERMIO
    /* Repasse en controle modem */
    if (cmodem) {
	term.c_cflag &= ~CLOCAL;
#ifdef USE_TERMIOS
	ioctl (fd, TCIOCSETA, &term);
#else
	ioctl (fd, TCSETA, &term);
#endif /* USE_TERMIOS */
    }
#endif

    if (q)
      *q = 0;

    return erreur;
}   


