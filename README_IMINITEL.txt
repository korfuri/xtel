                                [I-Minitel]

                                     et

                                  Xtel-3.3

                     Pierre Ficheux (pierre@alienor.fr)

                                F�vrier 2001

1. Qu'est-ce qu' I-Minitel ?

I-Minitel est un nouveau point d'acc�s rapide mis en place par France
Telecom. Cet acc�s n'utilise plus l'ancienne modulation V.23 (1200/75 bps)
mais un acc�s centralis� PPP sur le num�ro unique 3622. Nous rappelons que
le protocole PPP (Point to Point Protocol) est celui utilis� pour les
connexions Interne sur les fournisseurs d'acc�s habituels. Ce point d'acc�s
vous permet d'utiliser les services Minitel au d�bit maxi de votre modem
Internet (56 kbps et plus). Le r�seau IP I-Minitel est cependant
compl�tement ind�pendant du r�seau Internet et l'acc�s � I-Minitel sera
exclusivement disponible � travers cette connexion PPP m�me si vous disposez
d'une connexion Internet permanente.

Lorsque la page d'acc�s du 3622 apparait, il suffit d'entrer le code
habituel du service (SNCF, METEO, etc...). L'annuaire electronique est
�galement disponible � travers le 3622 en utilisant le code PAGESJAUNES.

Pour en savoir plus sur le produit I-Minitel, veuillez consulter le site
http://www.i-minitel.com.

2. I-Minitel et Xtel

Depuis sa conception en 1991, Xtel utilise une technique similaire � celle
de la technologie I-Minitel aujourd'hui � savoir l'acc�s � un service
Minitel � travers une connexion TCP/IP. Dans le cas de Xtel, le client xtel
effectue une connection TCP vers le serveur xteld, ce dernier effectuant la
connexion effective au service Minitel (le plus souvent par modem).

Dans le cas de I-Minitel, la connexion au service est TCP/IP d'un bout �
l'autre et la machine supportant xteld aura un role de routeur d'acc�s aux
services I-Minitel. En particuliers:

   * Le premier client xtel initialise la connexion PPP de xteld vers le
     serveur 3622. xteld mets alors en place la route vers le serveur
     I-Minitel

   * Les clients suivants utiliseront cette route sans avoir � rappeler le
     3622

   * La deconnexion du dernier client provoquera la coupure de la connexion
     PPP

   * Le client peut �tre un poste Windows �quip� d'un �mulateur de type
     HyperTerminal compatible TCP/IP

Pour effectuer la connexion PPP, xteld utilise le programme standard pppd.
L'acc�s I-Iminitel est d�fini dans le fichier xtel.services par une ligne du
type:

    @iminitel,I-Minitel,connect_iminitel:7516

Le param�tre connect_iminitel correspond � un shell-script de lancement de
la connexion PPP. Ce script est localis� au m�me endroit que le fichier
xtel.services (r�pertoire /usr/X11R6/lib/X11/xtel). Les fichiers de
configuration pppd sont les suivants:

 /etc/ppp/ip-up.iminitel  Script d'initialisation de la route I-Minitel,
                          ex�cut� � la connexion

 /etc/ppp/ip-down.iminitelScript ex�cut� lors de la coupure de connexion
                          I-Minitel
                          Chat-script de composition, contient le num�ro
 /etc/ppp/chat-iminitel   d'appel 3622 et les caract�ristiques
                          de la connexion PPP (login/password)

 /etc/ppp/peers/iminitel  Script de lancement de pppd, contient le nom de
                          la ligne s�rie utilis�e (par d�faut ttyS0)
