                                [I-Minitel]

                                     et

                                  Xtel-3.3

                     Pierre Ficheux (pierre@alienor.fr)

                                Février 2001

1. Qu'est-ce qu' I-Minitel ?

I-Minitel est un nouveau point d'accès rapide mis en place par France
Telecom. Cet accès n'utilise plus l'ancienne modulation V.23 (1200/75 bps)
mais un accès centralisé PPP sur le numéro unique 3622. Nous rappelons que
le protocole PPP (Point to Point Protocol) est celui utilisé pour les
connexions Interne sur les fournisseurs d'accès habituels. Ce point d'accès
vous permet d'utiliser les services Minitel au débit maxi de votre modem
Internet (56 kbps et plus). Le réseau IP I-Minitel est cependant
complètement indépendant du réseau Internet et l'accès à I-Minitel sera
exclusivement disponible à travers cette connexion PPP même si vous disposez
d'une connexion Internet permanente.

Lorsque la page d'accès du 3622 apparait, il suffit d'entrer le code
habituel du service (SNCF, METEO, etc...). L'annuaire electronique est
également disponible à travers le 3622 en utilisant le code PAGESJAUNES.

Pour en savoir plus sur le produit I-Minitel, veuillez consulter le site
http://www.i-minitel.com.

2. I-Minitel et Xtel

Depuis sa conception en 1991, Xtel utilise une technique similaire à celle
de la technologie I-Minitel aujourd'hui à savoir l'accès à un service
Minitel à travers une connexion TCP/IP. Dans le cas de Xtel, le client xtel
effectue une connection TCP vers le serveur xteld, ce dernier effectuant la
connexion effective au service Minitel (le plus souvent par modem).

Dans le cas de I-Minitel, la connexion au service est TCP/IP d'un bout à
l'autre et la machine supportant xteld aura un role de routeur d'accès aux
services I-Minitel. En particuliers:

   * Le premier client xtel initialise la connexion PPP de xteld vers le
     serveur 3622. xteld mets alors en place la route vers le serveur
     I-Minitel

   * Les clients suivants utiliseront cette route sans avoir à rappeler le
     3622

   * La deconnexion du dernier client provoquera la coupure de la connexion
     PPP

   * Le client peut être un poste Windows équipé d'un émulateur de type
     HyperTerminal compatible TCP/IP

Pour effectuer la connexion PPP, xteld utilise le programme standard pppd.
L'accès I-Iminitel est défini dans le fichier xtel.services par une ligne du
type:

    @iminitel,I-Minitel,connect_iminitel:7516

Le paramètre connect_iminitel correspond à un shell-script de lancement de
la connexion PPP. Ce script est localisé au même endroit que le fichier
xtel.services (répertoire /usr/X11R6/lib/X11/xtel). Les fichiers de
configuration pppd sont les suivants:

 /etc/ppp/ip-up.iminitel  Script d'initialisation de la route I-Minitel,
                          exécuté à la connexion

 /etc/ppp/ip-down.iminitelScript exécuté lors de la coupure de connexion
                          I-Minitel
                          Chat-script de composition, contient le numéro
 /etc/ppp/chat-iminitel   d'appel 3622 et les caractéristiques
                          de la connexion PPP (login/password)

 /etc/ppp/peers/iminitel  Script de lancement de pppd, contient le nom de
                          la ligne série utilisée (par défaut ttyS0)
