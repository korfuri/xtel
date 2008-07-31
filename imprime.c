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
    int fd = -1;
    char cmd[256];
    char n[] = "/var/tmp/xtelXXXXXX";
    /* crée le fichier de telle façon qu'il soit impossible de le détourner
     * avec un lien symbolique préalablement établi.
     */
#ifdef HAS_MKSTEMP
    fd = mkstemp(n);
#else
    if (mktemp(n) != NULL)
	fd = open (n, O_CREAT | O_EXCL | O_WRONLY, 0600);
#endif

    if (fd < 0) {
	perror (n);
    }
    else if ((fp = fdopen( fd, "wb" )) == NULL) {
	close (fd);
	unlink (n);
	perror (n);
    }
    else {
	if (mode == VIDEOTEX) {
	    videotexDumpScreen (ecran_minitel, fp);
#ifdef HAS_SNPRINTF
	    snprintf (cmd, sizeof(cmd), rsc_xtel.commandeImpression, n);
#else
	    sprintf (cmd, rsc_xtel.commandeImpression, n);
#endif
	}
	else { /* ASCII */
	    videotexConversionAscii (ecran_minitel, fp);
#ifdef HAS_SNPRINTF
	    snprintf (cmd, sizeof(cmd), rsc_xtel.commandeImpressionAscii, n);
#else
	    sprintf (cmd, rsc_xtel.commandeImpressionAscii, n);
#endif
	}

	fclose (fp);
	system (cmd);
	unlink (n);
    }
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
