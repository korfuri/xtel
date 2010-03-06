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
static char rcsid[] = "$Id: imprime.c,v 1.6 1998/10/02 15:04:32 pierre Exp $";

#include "xtel.h"

/*
 * Imprime la page courante
 */
static void imprime_page_courante (mode)
int mode;
{
    FILE *fp;
    char cmd[256], n[256];

    sprintf (n, "/tmp/xtel%d.ppm", getpid());
    if ((fp = fopen (n, "w")) == NULL) {
	perror (n);
	exit (1);
    }

    if (mode == VIDEOTEX) {
	videotexDumpScreen (ecran_minitel, fp);
	sprintf (cmd, rsc_xtel.commandeImpression, n);
    }
    else { /* ASCII */
	videotexConversionAscii (ecran_minitel, fp);
	sprintf (cmd, rsc_xtel.commandeImpressionAscii, n);
    }

    fclose (fp);
    system (cmd);
    unlink (n);
}

void imprime_page_courante_ascii (w, client_data, call_data)
Widget w;
XtPointer client_data, call_data;
{
    imprime_page_courante (ASCII);
}

void imprime_page_courante_videotex (w, client_data, call_data)
Widget w;
XtPointer client_data, call_data;
{
    imprime_page_courante (VIDEOTEX);
}
