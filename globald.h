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
#ident "$Id: globald.h,v 1.9 1998/10/02 15:02:56 pierre Exp $"

#ifndef _globald_h
#define _globald_h

typedef char Boolean;
#define True	1
#define False	0

/*
 * pour récupérer la config globale du programme
 */
#include "Config.tmpl"

#ifndef EXTERN
#define EXTERN extern
#endif

/*
 * Variables
 */
#ifndef HAS_STRERROR
extern char *sys_errlist[];
#define strerror(e) (sys_errlist[e])
#endif

EXTERN struct definition_ligne definition_lignes[MAX_LIGNES]; 
EXTERN struct definition_service definition_services[MAX_SERVICES];
EXTERN Boolean flag_m1;
EXTERN int fd_modem;
EXTERN int nb_lignes;
EXTERN char numero_ligne;

extern char version_xtel;
extern char revision_xtel;

/*
 * Fonctions
 */
#ifdef __STDC__

/* xteld.c */
void erreur_a_xtel(char *s, int code_erreur);
int service_autorise(int indice_service, int *);
void appel_service(char *service_teletel);
int main(int ac, char **av);
/* dial.c */
void myundial(int fd);
int mydial(char *telno, char *device);
/* config.c */
int lecture_services(void);
int lecture_configuration_lignes(void);
/* misc.c */
char *next_token(char *, char *);
char *build_name(char *);
char *xtel_basename(char *s);
/* ian.c */
int ian_valide (int, char);
void ian_init (char*);

/* modem.c */
void init_tty (int, int, int, int, int, int, int);
void restore_tty (int);
int do_chat (int, char*, unsigned long, int, char*, char*, int);
void init_debug (char*);
void close_debug (void);
#else

void erreur_a_xtel();
int service_autorise();
void appel_service();
int main();
void myundial();
int mydial();
int lecture_services();
int lecture_configuration_lignes();
char *next_token();
char *build_name();
int ian_valide ();
void ian_init ();
char *xtel_basename();
void init_tty ();
void restore_tty ();
int do_chat ();
void init_debug ();
void close_debug ();
FILE *get_fp_console ();

#endif /* __STDC__ */

#endif
