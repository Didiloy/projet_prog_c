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
#include <math.h>
#include <sys/wait.h>

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

static void parseArgs(int argc, char *argv[] /*, structure à remplir*/)
{
    if (argc != 4)
    {
        usage(argv[0], "Nombre d'arguments incorrect");
    }

    // remplir la structure
    donnee.aSuite = false;
    donnee.valeurAssocie = atoi(argv[1]);
    donnee.fdToWorker = atoi(argv[2]);
    donnee.fdToMaster = atoi(argv[3]);
    // fprintf(stderr, "valeur associe %d fdToWorker %d fdToMaster %d\n", donnee.valeurAssocie, donnee.fdToWorker, donnee.fdToMaster);
}

bool pasPremier(int val)
{
    return val % donnee.valeurAssocie == 0;
}

int creerSuite(int val)
{
    // TODO créer suite et retourne le tube pour communiquer avec lui
    int fds[2];
    int ret = pipe(fds);
    myassert(ret != -1, "Impossible de créer le tube ecriture du worker vers le worker\n");

    ret = fork();
    if (ret == 0)
    {
        close(fds[1]);

        char lecture[(int)((ceil(log10(fds[0])) + 1) * sizeof(char))]; // déclarer un tableau de caractère de la bonne taille
        sprintf(lecture, "%d", fds[0]);

        char ecriture[(int)((ceil(log10(donnee.fdToMaster)) + 1) * sizeof(char))]; // déclarer un tableau de caractère de la bonne taille
        sprintf(ecriture, "%d", donnee.fdToMaster);

        char valeur[(int)((ceil(log10(val)) + 1) * sizeof(char))];
        sprintf(valeur, "%d", val);

        char *args[5];

        args[0] = "worker";
        args[1] = valeur;
        args[2] = lecture;
        args[3] = ecriture;
        args[4] = NULL;

        execv("worker", args);
    }
    else
    {
        close(fds[0]);

        donnee.aSuite = true;
    }
    return fds[1];
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
        read(donnee.fdToWorker, &order, sizeof(int));

        if (order == W_ORDER_STOP)
        {
            if (donnee.aSuite)
            {
                write(donnee.workerToWorker, &order, sizeof(int));
                wait(NULL);
                close(donnee.workerToWorker);
                close(donnee.fdToWorker);
                break;
            }
            else
            {
                close(donnee.fdToWorker);
                break;
            }
        }
        else
        {
            if (order == donnee.valeurAssocie)
            {
                int ret = W_IS_PRIME;
                write(donnee.fdToMaster, &ret, sizeof(int));
            }
            else if (pasPremier(order))
            {
                int ret = W_IS_NOT_PRIME;
                write(donnee.fdToMaster, &ret, sizeof(int));
            }
            else if (donnee.aSuite)
            {

                write(donnee.workerToWorker, &order, sizeof(int));
            }
            else
            {
                donnee.workerToWorker = creerSuite(order);
            }
        }
    }
}

/************************************************************************
 * Programme principal
 ************************************************************************/

int main(int argc, char *argv[])
{

    parseArgs(argc, argv /*, structure à remplir*/);

    // Si on est créé c'est qu'on est un nombre premier
    // Envoyer au master un message positif pour dire
    // que le nombre testé est bien premier
    int a = W_IS_PRIME;

    int ret = write(donnee.fdToMaster, &a, sizeof(int));
    myassert(ret != -1, "Impossible d'écrire au master");

    loop();

    // libérer les ressources : fermeture des files descriptors par exemple

    return EXIT_SUCCESS;
}
