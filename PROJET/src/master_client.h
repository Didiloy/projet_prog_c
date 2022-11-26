/**
* @authors Lacaze Yon - Loya Dylan
*/
#ifndef CLIENT_CRIBLE
#define CLIENT_CRIBLE

// On peut mettre ici des éléments propres au couple master/client :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (création tubes, écriture dans un tube,
//      manipulation de sémaphores, ...)

// ordres possibles pour le master
#define ORDER_NONE 0
#define ORDER_STOP -1
#define ORDER_COMPUTE_PRIME 1
#define ORDER_HOW_MANY_PRIME 2
#define ORDER_HIGHEST_PRIME 3
#define ORDER_COMPUTE_PRIME_LOCAL 4 // ne concerne pas le master

// Reponses possibles du master au client
#define STOPPED 5
#define M_NUMBER_IS_PRIME 6
#define M_NUMBER_IS_NOT_PRIME 7

#define NOM_FICHIER "master_client.h"
#define NUMERO 1

#define NOM_FICHIER_TUBE "master.c"
#define NUMERO_TUBE 3

#define NOM_FICHIER_TABLEAU "master.c"
#define NUMERO_TABLEAU 3

#define ECRITURE_MASTER_CLIENT "ecriture_master_client"
#define MODE 0641
#define LECTURE_MASTER_CLIENT "lecture_master_client"

// bref n'hésitez à mettre nombre de fonctions avec des noms explicites
// pour masquer l'implémentation

#endif
