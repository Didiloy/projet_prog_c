#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "myassert.h"

#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un worker
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le worker
// a besoin : le nombre premier dont il a la charge, ...

DonneeWorker donnee;


/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s <n> <fdIn> <fdToMaster>\n", exeName);
    fprintf(stderr, "   <n> : nombre premier géré par le worker\n");
    fprintf(stderr, "   <fdIn> : canal d'entrée pour tester un nombre\n");
    fprintf(stderr, "   <fdToMaster> : canal de sortie pour indiquer si un nombre est premier ou non\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static void parseArgs(int argc, char * argv[] /*, structure à remplir*/)
{
    if (argc != 4)
        usage(argv[0], "Nombre d'arguments incorrect");

    // remplir la structure
    donnee.aSuite = false;
    donnee.valeurAssocie = atoi(argv[2]);
    donnee.fdToWorker = atoi(argv[3]);
    donnee.fdToMaster = atoi(argv[4]);

}

bool pasPremier(int val){
    return val%donnee.valeurAssocie == 0;

}

int creerSuite(int val){
    //TODO créer suite et retourne le tube pour communiquer avec lui
}

/************************************************************************
 * Boucle principale de traitement
 ************************************************************************/

void loop()
{
    // boucle infinie :
    //    attendre l'arrivée d'un nombre à tester
    //    si ordre d'arrêt
    //       si il y a un worker suivant, transmettre l'ordre et attendre sa fin
    //       sortir de la boucle
    //    sinon c'est un nombre à tester, 4 possibilités :
    //           - le nombre est premier
    //           - le nombre n'est pas premier
    //           - s'il y a un worker suivant lui transmettre le nombre
    //           - s'il n'y a pas de worker suivant, le créer

    while (true)
    {
        int order;
        read(donnee.fdToWorker,&order,sizeof(int));
        if(order == W_ORDER_STOP){
            if(donnee.aSuite){
                write(donnee.workerToWorker, &order, sizeof(int) );
                //TODO suite
            }
            else{
                int ret = W_STOPPED;
                write(donnee.fdToMaster,&ret,sizeof(int));
                break;
            }
        }
        else{
            if(order == donnee.valeurAssocie){
                int ret = 6; /*a définir dans master_worker*/
                write(donnee.fdToMaster,&ret,sizeof(int));
            }
            else if(pasPremier(order)){
                int ret = 7; /*a définir dans master_worker*/
                write(donnee.fdToMaster,&ret,sizeof(int));
            }
            else if(donnee.aSuite){
                
                write(donnee.workerToWorker,&order,sizeof(int));
            }
            else{
                donnee.workerToWorker = creerSuite(order);
            }
        }

    }
    
}

/************************************************************************
 * Programme principal
 ************************************************************************/

int main(int argc, char * argv[])
{
    parseArgs(argc, argv /*, structure à remplir*/);
    
    // Si on est créé c'est qu'on est un nombre premier
    // Envoyer au master un message positif pour dire
    // que le nombre testé est bien premier
    int a = 6; /*a définir dans master_worker*/
    write(donnee.fdToMaster,&a,sizeof(int));


    loop();

    // libérer les ressources : fermeture des files descriptors par exemple


    return EXIT_SUCCESS;
}
