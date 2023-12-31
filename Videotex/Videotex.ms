\"
\" Doc du widget Videotex (macros ms)
\" $Id: Videotex.ms,v 1.3 1996/10/31 13:00:03 pierre Exp $
\" 
.if n \{
.nr LL 79n
.ll 79n
.nr FL 79n
.nr LT 79n
.\}
.nr PS 11
.de Ds
.LP
.in +0.6i
.LP
..
.de De
.in -0.6i
..
.de Nf
.cs R 20
\s-2
.nf
..
.de Fi
\s+2
.fi
.cs R
..
\"
\"
.ce 7
\s+5\fBWidget Videotex\fP\s-5
.sp
Octobre 1996
.sp
\fIPierre Ficheux\fP
.sp
LECTRA SYSTEMES
.sp 0
Service Recherche et D�veloppement
.sp 0
Z.I. Marticot 33610 CESTAS
.sp
E-mail: pierre@rd.lectra.fr
.sp 4
.NH
Pr�sentation
.PP
Le widget \fBVideotex\fP permet de cr�er dans une application X utilisant la 
librairie des \fIIntrinsics\fP (Xt) un �cran d'�mulation vid�otex (compatible 
Minitel 2). L'�mulation fonctionne sur un �cran couleur, monochrome ou � niveaux de
gris.
.PP
Le mode \fI80 colonnes\fP (mode \fIt�l�-informatique\fP) du minitel 
n'est pas g�r� par le widget (il doit �tre g�r� dans l'application). On peut 
utiliser ce widget dans une application MOTIF si l'on compile avec le flag
\fI-DUSE_MOTIF\fP. La diff�rence r�side principalement dans la d�finition des
\fIKeySyms\fP (qui sont sp�ciaux sous MOTIF).
.PP
Le programme suivant (pr�sent sur le r�pertoire \fIVideotex\fP) peut �tre consid�r� 
comme un �mulateur minimal (d�codage d'un fichier vid�otex) :
.sp
.Nf

/*
 * Test du widget Videotex
 */
#include <stdio.h>
#include <fcntl.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>

#include "Videotex.h"

Widget toplevel, form, vdts;
XtAppContext app_context;

static void Fin (w, client_data, pevent)
Widget w;
caddr_t client_data;
XButtonEvent *pevent;
{
    exit (0);
}

static void Decode (w, client_data, pevent)
Widget w;
caddr_t client_data;
XButtonEvent *pevent;
{
    char c;
    int fd;

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

main (ac, av)
int ac;
char **av;
{
    if (ac == 1) {
	fprintf (stderr, "Usage: xvdt fichier_videotex\n");
	exit (1);
    }

    toplevel = XtAppInitialize(&app_context, "XVdt", NULL, 0, &ac, av, NULL, NULL, 0); 
    form = XtCreateManagedWidget ("form", formWidgetClass, toplevel, NULL, 0);
    vdts = XtCreateManagedWidget ("vdts", videotexWidgetClass, form, NULL, 0);

    XtAddEventHandler (vdts, ExposureMask, False, Decode, (caddr_t)av[1]);
    XtAddEventHandler (vdts, ButtonPressMask, False, Fin, NULL);

    XtRealizeWidget(toplevel);
    XtAppMainLoop (app_context);
}

.Fi
.NH
La classe Videotex
.NH 2
Introduction
.PP
Le widget Videotex impl�mente la classe \fBvideotexWidgetClass\fP. La structure du
widget est conforme � la structure standard des widgets Xt:
.sp
.IP "Videotex.c" 2i
le source du widget
.IP "VideotexP.h" 2i
le fichier d'en-t�te \fIpriv�\fP du widget
.IP "Videotex.h" 2i
le fichier d'en-t�te \fIpublic\fP du widget
.sp
.PP
Le widget Vid�otex peut donc �tre utilis� dans une application \fIOSF-Motif\fP ou
bien \fIAthena (Xaw)\fP. 
.PP
Le widget utilise des fontes sp�cifiques (fontes vid�otex) fournies dans la 
distribution de \fIxtel-3.0\fP sur le repertoire \fPfonts\fI. 
.sp
.NH 2
Les ressources du widget
.PP
.IN "Panner widget" "resources"
.ps 9
.nr PS 9
.vs 11
.nr VS 11
.TS H
expand;
lw(1.5i) lw(1.5i) lw(1i) lw(2i).
_
.sp 3p
.TB
Name	Class	Type	Default Value
.sp 3p
_
.TH
.R
.sp 3p
petiteFonte	PetiteFonte	Boolean	False
nGris	NGris	Boolean	False
connecte	Connecte	Boolean	False
fdConnexion	FdConnexion	int	-1
commandeDeconnexion	CommandeDeconnexion	String	NULL
modeCallback	ModeCallback	Callback	NULL
enregCallback	EnregCallback	Callback	NULL
modeVideotex	ModeVideotex	Boolean	True
enqROM	EnqROM	String	Cv1
.sp 3p
_
.TE
.ps 11
.nr PS 11
.vs 13
.nr VS 13
.Ac
.IP petiteFonte 2i
utilisation de la fonte 8x10 points � la place de la fonte 16x20.
.sp
.IP nGris 2i
l'�mulation utilise des niveaux de gris � la place des couleurs (sauf sur 
un �cran monochrome bien s�r !)
.sp
.IP connecte 2i
indique au widget si il est connect� � la ligne ou non
.sp
.IP fdConnexion 2i
valeur du \fIfile-descriptor\fP Unix associ� � la connexion
.sp
.IP commandeDeconnexion 2i
chaine de caract�res � envoyer sur le \fIdescripteur de fichier\fP pr�c�dent 
sur une demande de d�connexion modem.
.sp
.IP modeCallback 2i
callback appel� lors du passage en mode t�l�-informatique
.sp
.IP enregCallback 2i
callback appel� si l'on veut enregistrer les commandes Minitel (SUITE, ENVOI, 
etc...)
.sp
.IP modeVideotex 2i
mode de fonctionnement (Vid�otex si True)
.sp
.IP enqROM 2i
identificateur � renvoyer sur un ENQROM (le widget ajoute SOH et EOT)
.sp
.NH 2
Les actions du widget
.sp
.PP
Le widget \fBVideotex\fP reconnait les actions suivantes dans le cas de 
l'utilisation du toolkit Athena :
.sp
.Nf
<Btn1Down>:	reconnaissance-commande()
<Btn3Down>:	conversion-ascii-page()
<Key>Return:	emission-commande(A)
<Key>Prior:	emission-commande(B)
<Key>Insert:	emission-commande(C)
<Key>End:	emission-commande(D)
<Key>Delete:	emission-commande(E)
<Key>Home:	emission-commande(F)
Ctrl<Key>H:	emission-commande(G)
<Key>Next:	emission-commande(H)
.Fi
.PP
Il reconnait les actions suivantes dans le cas de MOTIF :
.sp
.Nf
<Btn1Down>:		reconnaissance-commande()
<Btn3Down>:		conversion-ascii-page()
<Key>Return:		emission-commande(A)
<Key>osfPageUp:		emission-commande(B)
<Key>osfInsert:		emission-commande(C)
<Key>osfEndLine:	emission-commande(D)
<Key>osfDelete:		emission-commande(E)
<Key>osfBeginLine:	emission-commande(F)
Ctrl<Key>H:		emission-commande(G)
<Key>osfPageDown:	emission-commande(H)
.Fi
.sp
.IP reconnaissance-commande() 2i
le clic avec le bouton 1 dans une zone de l'�cran Vid�otex retourne la chaine de
caract�res compl�te point�e par la souris (on tente de reconnaitre les commandes
du Minitel \fIENVOI\fP, \fIRETOUR\fP, ...)
.sp
.IP emission_commande(chaine) 2i
permet d'associer des touches du clavier aux commandes du clavier minitel
.sp
.NH 2
Les fonctions publiques du widget
.sp
.PP
.FD 0
void videotexInit(\fIw\fP)
.br
Widget \fIw\fP;
.FN
.IP \fIw\fP 1i
sp�cifie le widget Videotex
.PP
Initialise l'�mulation dans le widget
.sp 2
.FD 0
void videotexDecode(\fIw\fP, \fIc\fP)
.br
Widget \fIw\fP;
.br
char \fIc\fP;
.FN
.IP \fIw\fP 1i
sp�cifie le widget Videotex
.IP \fIc\fP 1i
le caract�re � d�coder
.PP
Effectue le d�codage du caract�re \fIc\fP dans l'�cran du widget
Videotex \fIw\fP
.sp 2
.FD 0
Pixmap videotexPixmapEcranSauve (\fIw\fP)
.br
Widget \fIw\fP;
.FN
.IP \fIw\fP 1i
sp�cifie le widget Videotex
.PP
Retourne le \fIPixmap\fP de sauvegarde de l'�cran (utilis� si le serveur X ne fait
pas de \fIbacking-store\fP)
.sp 2
.FD 0
void videotexSauveEcran (\fIw\fP)
.br
Widget \fIw\fP;
.FN
.IP \fIw\fP 1i
sp�cifie le widget Videotex
.PP
Force la sauvegarde de l'�cran vid�otex
