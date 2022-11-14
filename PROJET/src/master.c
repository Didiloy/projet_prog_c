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
#include <math.h>
#include <fcntl.h>
#include "myassert.h"

#include "master_client.h"
#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un master
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le master
// a besoin

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s\n", exeName);
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

/************************************************************************
 * boucle principale de communication avec le client
 ************************************************************************/
void loop(int writeToWorker, int receiveFromWorker)
{
    printf("DANS la boucle \n");
    // boucle infinie :
    bool infini = true;
    while (infini)
    {
        // - ouverture des tubes (cf. rq client.c)
        int tubeLectureClient = open(LECTURE_CLIENT, O_WRONLY);
        myassert(tubeLectureClient != -1, "Impossible d'ouvrir le tube de lecture du client\n");

        int tubeEcritureClient = open(ECRITURE_CLIENT, O_RDONLY);
        myassert(tubeEcritureClient != -1, "Impossible d'ouvrir le tube d'écriture pour le client\n");

        // - attente d'un ordre du client (via le tube nommé)
        int order;
        ssize_t ret = read(tubeLectureClient, &order, sizeof(int));
        myassert(ret != -1, "Impossible de lire dans le tube de lecture du client\n");

        int confirmFromWorker, res, orderToSend;
        printf("j'ai recu un ordre du client %d\n", order);

        switch (order)
        {
        // - si ORDER_STOP
        //       . envoyer ordre de fin au premier worker et attendre sa fin
        //       . envoyer un accusé de réception au client
        case ORDER_STOP:
            orderToSend = W_ORDER_STOP;
            res = write(writeToWorker, &orderToSend, sizeof(int));
            myassert(res != -1, "Impossible d'envoyer un ordre au worker depuis le master\n");
            res = read(receiveFromWorker, &confirmFromWorker, sizeof(int));
            myassert(res != -1, "Impossible de recevoir un message du worker dans le master\n'");
            if (confirmFromWorker == W_STOPPED)
            {
                orderToSend = STOPPED;
                res = write(tubeEcritureClient, &orderToSend, sizeof(int));
                myassert(res != -1, "Impossible d'écrire au client depuis le master\n");
            }
            else
            {
                // TODO si le worker ne peut pas s'arreter
            }
            infini = false;
            break;

        // - si ORDER_COMPUTE_PRIME
        //       . récupérer le nombre N à tester provenant du client
        //       . construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
        //             il faut connaître le plus nombre (M) déjà enovoyé aux workers
        //             on leur envoie tous les nombres entre M+1 et N-1
        //             note : chaque envoie déclenche une réponse des workers
        //       . envoyer N dans le pipeline
        //       . récupérer la réponse
        //       . la transmettre au client
        case ORDER_COMPUTE_PRIME:
            break;

        // - si ORDER_HOW_MANY_PRIME
        //       . transmettre la réponse au client
        case ORDER_HOW_MANY_PRIME:
            break;

        // - si ORDER_HIGHEST_PRIME
        //       . transmettre la réponse au client
        // - fermer les tubes nommés
        // - attendre ordre du client avant de continuer (sémaphore : précédence)
        // - revenir en début de boucle
        case ORDER_HIGHEST_PRIME:
            break;

        case ORDER_NONE:
            break;
        }
        //
        // il est important d'ouvrir et fermer les tubes nommés à chaque itération
        // voyez-vous pourquoi ?
    }
}

/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char *argv[])
{
    if (argc != 1)
        usage(argv[0], NULL);

    // - création des sémaphores
    key_t cleClient = ftok(NOM_FICHIER, NUMERO);
    myassert(cleClient != -1, "Impossible de créer la clé\n");

    int semClient = semget(cleClient, 1, IPC_CREAT | IPC_EXCL | 0641);
    myassert(semClient != -1, "Impossible de créer le sémaphore client\n");

    int semTableau = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | 0641);
    myassert(semTableau != -1, "Impossible de créer le sémaphore tableau\n");

    // - création des tubes nommés
    int ret = mkfifo(ECRITURE_CLIENT, MODE);
    myassert(ret != -1, "Impossible de créer le tube ecriture client\n");

    ret = mkfifo(LECTURE_CLIENT, MODE);
    myassert(ret != -1, "Impossible de créer le tube lecture client\n");

    // - création du premier worker

    int fdToMaster[2];
    ret = pipe(fdToMaster);
    myassert(ret != -1, "Impossible de créer le tube ecriture vers le master du worker\n");
    close(fdToMaster[0]);

    int fds[2];
    ret = pipe(fds);
    myassert(ret != -1, "Impossible de créer le tube lecture du précédent worker du worker\n");
    close(fds[1]);

    ret = fork();
    if (ret == 0)
    {
        char lecture[(int)((ceil(log10(fds[0])) + 1) * sizeof(char))]; // déclarer un tableau de caractère de la bonne taille
        sprintf(lecture, "%d", fds[0]);

        char ecriture[(int)((ceil(log10(fdToMaster[1])) + 1) * sizeof(char))]; // déclarer un tableau de caractère de la bonne taille
        sprintf(ecriture, "%d", fdToMaster[1]);

        /**
         * Ordre des paramètres du worker
         * - le nombre dont il va s'occuper
         * - le tube pour recevoir du worker d'avant
         * - le tube pour écrire au master
         */
        char *args[5];
        args[0] = "worker";
        args[1] = "2";
        args[2] = lecture;
        args[3] = ecriture;
        args[4] = "\0";

        execv("worker", args);
        
    }
    else
    {
        // boucle infinie
        loop(fds[0], fdToMaster[1]);

        // destruction des tubes nommés, des sémaphores, ...
        ret = semctl(semClient, 0, IPC_RMID);
        myassert(ret != -1, "Impossible de supprimer le sémaphore client\n");

        ret = semctl(semTableau, 0, IPC_RMID);
        myassert(ret != -1, "Impossible de supprimer le sémaphore tableau\n");

        ret = unlink(ECRITURE_CLIENT);
        myassert(ret != -1, "Impossible de supprimer le tube ecriture client\n");

        ret = unlink(LECTURE_CLIENT);
        myassert(ret != -1, "Impossible de supprimer le tube lecture client\n");
    }

    

    return EXIT_SUCCESS;
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien
