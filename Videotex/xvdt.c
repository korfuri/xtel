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
 */
static char rcsid[] = "$Id: xvdt.c,v 1.5 1998/10/02 14:51:37 pierre Exp $";

/*
 * Test du widget Videotex
 */
 
#include <stdio.h>
#include <fcntl.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>

#include "Videotex.h"

extern char *basename ();

static Widget toplevel, form, vdts;
static XtAppContext app_context;
static Colormap new_cmap;
static int fd;
static XtInputId input_id;
static char *xteld_serveur, *xteld_service;

static void Fin (w, client_data, pevent)
Widget w;
caddr_t client_data;
XButtonEvent *pevent;
{
  if (xteld_serveur) {
    XtRemoveInput (input_id);
    close (fd);
  }
  exit (0);

}

void read_the_server (client_data, fid, id)
XtPointer client_data;	/* non utilise */
int *fid;
XtInputId *id;
{
    unsigned char c;

    if (read (*fid, &c, 1) > 0)
      videotexDecode (vdts, c);
}

static void OpenConnection (w, client_data, pevent)
Widget w;
caddr_t client_data;
XButtonEvent *pevent;
{
    char c;

    if ((fd = c_clientbyname (xteld_serveur, xteld_service)) < 0) {
      fprintf (stderr, "OpenConnection: Erreur de connexion au serveur %s, service%s.\n", xteld_serveur, xteld_service);
      exit (1);
    }

    XtVaSetValues (vdts, XtNfdConnexion, fd, NULL);
    input_id = XtAppAddInput (app_context, fd, (XtPointer)XtInputReadMask, (XtInputCallbackProc)read_the_server, NULL);
}    

static void Decode (w, client_data, pevent)
Widget w;
caddr_t client_data;
XButtonEvent *pevent;
{
    char c;

    fd = open (client_data, O_RDONLY);
    
    if (fd > 0) {
	XtVaSetValues (w, XtNfdConnexion, fd, NULL);
	videotexDecode (w, 12);
	while (read (fd, &c, 1) > 0)
	    videotexDecode (w, c);
	close (fd);
    }
    else {
	perror (client_data);
	exit (1);
    }
}

static void Usage (char *s)
{
  fprintf (stderr, "%s: [-p] [-s serveur] [-S service] [fichier]\n", s);
  exit (1);
}

main (ac, av)
int ac;
char **av;
{
    char *filename, *progname, *cp;
    Boolean private_cmap = False;
    int i;

    progname = basename(av[0]);

    toplevel = XtAppInitialize(&app_context, "XVdt", NULL, 0, &ac, av, NULL, NULL, 0); 

    if (ac < 2)
      Usage (progname);

    /* Lecture de la ligne de commande */
    while (--ac) {
	if ((cp = *++av) == NULL)
	  break;
	if (*cp == '-' && *++cp) {
	  switch(*cp) {

	  case 'p' :
	    ++av;
	    private_cmap = True;
	    break;

	  case 's' :
	    xteld_serveur = *++av; break;

	  case 'S':
	    xteld_service = *++av; break;

	  default:
	    Usage (progname);
	  }
	}
	else {
	    break;
	}
    }
    
    filename = cp;
    if (xteld_serveur && !xteld_service)
      xteld_service = "xtel";

    if (private_cmap) {
	new_cmap = XCopyColormapAndFree (XtDisplay(toplevel), DefaultColormapOfScreen ( XtScreen( toplevel)) );
	XtVaSetValues (toplevel, XtNcolormap, new_cmap, NULL);
    }

    form = XtCreateManagedWidget ("form", formWidgetClass, toplevel, NULL, 0);
    vdts = XtVaCreateManagedWidget ("vdts", videotexWidgetClass, form, XtNenqROM, "Cv1", XtNconnecte, True, NULL);

    if (xteld_serveur)
      XtAddEventHandler (vdts, ExposureMask, False, OpenConnection, (caddr_t)NULL);
    else
      XtAddEventHandler (vdts, ExposureMask, False, Decode, (caddr_t)filename);

    XtAddEventHandler (vdts, ButtonPressMask, False, Fin, NULL);

    XtRealizeWidget(toplevel);
    XtAppMainLoop (app_context);
}
