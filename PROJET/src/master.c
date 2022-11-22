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
void loop(int writeToWorker, int receiveFromWorker, int semClient,int semTubeClient)
{
    int m = 2; // Plus grand nombre envoyé aux worker. 2 de base car on crée le premier worker avec 2
    int nombreDeNombreCalcule = 0;
    int plusGrandNombrePremierCalcule = 0;
    // boucle infinie :
    bool infini = true;
    while (infini)
    {
        printf("[MASTER] en attente d'ordre d'un du client... \n");
        // - ouverture des tubes (cf. rq client.c)
        int tubeLectureClient = open(LECTURE_MASTER_CLIENT, O_RDONLY);
        myassert(tubeLectureClient != -1, "Impossible d'ouvrir le tube de lecture du client\n");

        int tubeEcritureClient = open(ECRITURE_MASTER_CLIENT, O_WRONLY);
        myassert(tubeEcritureClient != -1, "Impossible d'ouvrir le tube d'écriture pour le client\n");

        // - attente d'un ordre du client (via le tube nommé)
        int order;
        ssize_t ret = read(tubeLectureClient, &order, sizeof(int));
        myassert(ret != -1, "Impossible de lire dans le tube de lecture du client\n");

        int res;
        // printf("j'ai recu un ordre du client %d\n", order);

        switch (order)
        {
        // - si ORDER_STOP
        //       . envoyer ordre de fin au premier worker et attendre sa fin
        //       . envoyer un accusé de réception au client
        case ORDER_STOP:
            orderStop(writeToWorker, receiveFromWorker, tubeEcritureClient);
            struct sembuf op = {0, -1, 0};
            int r = semop(semClient, &op, 1);
            myassert(r != -1, "Impossible de retirer 1 au semaphore.");
            infini = false;
            break;

        // - si ORDER_COMPUTE_PRIME
        //       . récupérer le nombre N à tester provenant du client
        //       . construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
        //             il faut connaître le plus grand nombre (M) déjà envoyé aux workers
        //             on leur envoie tous les nombres entre M+1 et N-1
        //             note : chaque envoie déclenche une réponse des workers
        //       . envoyer N dans le pipeline
        //       . récupérer la réponse
        //       . la transmettre au client
        case ORDER_COMPUTE_PRIME:
            res = orderComputePrime(writeToWorker, receiveFromWorker, tubeEcritureClient, tubeLectureClient, &m, &plusGrandNombrePremierCalcule);
            // ajouter +1 au nombre de nombres premier calculés par le master pour pouvoir l'envoyer en cas de demande du client
            nombreDeNombreCalcule += 1;

            break;

        // - si ORDER_HOW_MANY_PRIME
        //       . transmettre la réponse au client
        case ORDER_HOW_MANY_PRIME:
            // Envoyer au client le nombre de nombre premuer calculés
            res = write(tubeEcritureClient, &nombreDeNombreCalcule, sizeof(int));
            myassert(res != -1, "Impossible d'écrire au client depuis le master\n");
            break;

        // - si ORDER_HIGHEST_PRIME
        //       . transmettre la réponse au client
        // - fermer les tubes nommés
        // - attendre ordre du client avant de continuer (sémaphore : précédence)
        // - revenir en début de boucle
        case ORDER_HIGHEST_PRIME:
            // Envoyer au client le plus grand nombre premier calculé
            res = write(tubeEcritureClient, &plusGrandNombrePremierCalcule, sizeof(int));
            myassert(res != -1, "Impossible d'écrire au client depuis le master\n");
            break;

        case ORDER_NONE:
            break;
        }
        //
        // il est important d'ouvrir et fermer les tubes nommés à chaque itération
        // voyez-vous pourquoi
        // - fermeture des tubes
        res = close(tubeLectureClient);
        myassert(res != -1, "Impossible de fermer le tube lecture client");

        res = close(tubeEcritureClient);
        myassert(res != -1, "Impossible de fermer le tube écriture client");

        struct sembuf operation;
        operation.sem_num = 0;
        operation.sem_op = -1;
        operation.sem_flg = 0;

        ret = semop(semTubeClient, &operation, 1);
        myassert(ret != -1, "Impossible de faire une opération sur le sémaphore depuis le master");
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
    // vu avec Daniel Menevaux : sémaphore
    myassert(semClient != -1, "Impossible de créer le sémaphore client\n");
    // printf("%d\n", semClient);
    // mettre le sem a 1
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = +1;
    op.sem_flg = 0;
    int r = semop(semClient, &op, 1);
    myassert(r != -1, "Impossible de passer le semaphore a 1.");

    key_t cleTubeClient = ftok(NOM_FICHIER_TUBE, NUMERO_TUBE);
    myassert(cleTubeClient != -1, "Impossible de créer la clé\n");

    int semTubeClient = semget(cleTubeClient, 1, IPC_CREAT | IPC_EXCL | 0641);
    // vu avec Daniel Menevaux : sémaphore
    myassert(semTubeClient != -1, "Impossible de créer le sémaphore client\n");

    
    // printf("%d\n", semTableau);
    // TODO peut etre passer le semaphore a 1

    // - création des tubes nommés
    int ret = mkfifo(ECRITURE_MASTER_CLIENT, MODE);
    myassert(ret != -1, "Impossible de créer le tube ecriture client\n");

    ret = mkfifo(LECTURE_MASTER_CLIENT, MODE);
    myassert(ret != -1, "Impossible de créer le tube lecture client\n");

    // - création du premier worker

    int fdToMaster[2];
    ret = pipe(fdToMaster);
    myassert(ret != -1, "Impossible de créer le tube écriture du worker vers le master\n");

    // fprintf(stderr, "fdMaster ecriture : %d ,  fdMaster lecture : %d\n", fdToMaster[1], fdToMaster[0]);

    int fds[2];
    ret = pipe(fds);
    myassert(ret != -1, "Impossible de créer le tube ecriture du master vers le worker\n");

    // fprintf(stderr, "fds ecriture : %d ,  fds lecture : %d\n", fds[1], fds[0]);

    ret = fork();
    if (ret == 0)
    {
        close(fds[1]);
        close(fdToMaster[0]);
        char lecture[(int)((ceil(log10(fds[0])) + 1) * sizeof(char))]; // déclarer un tableau de caractère de la bonne taille
        sprintf(lecture, "%d", fds[0]);

        char ecriture[(int)((ceil(log10(fdToMaster[1])) + 1) * sizeof(char))]; // déclarer un tableau de caractère de la bonne taille
        sprintf(ecriture, "%d", fdToMaster[1]);

        // printf("lecture : %s ,  ecriture : %s\n", lecture, ecriture);

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
        args[4] = NULL;

        execv("worker", args);
    }
    else
    {
        close(fds[0]);
        close(fdToMaster[1]);
        int tmp;
        // fprintf(stderr, "Avant le read\n");
        ret = read(fdToMaster[0], &tmp, sizeof(int));
        myassert(ret != -1, "Impossible de récupérer la réponse du worker");

        fprintf(stderr, "Premier worker bien lancé\n");

        // boucle infinie
        loop(fds[1], fdToMaster[0], semClient, semTubeClient);
        // destruction des tubes nommés, des sémaphores, ...
        ret = semctl(semClient, 0, IPC_RMID);
        myassert(ret != -1, "Impossible de supprimer le sémaphore client\n");

        ret = semctl(semTubeClient, 0, IPC_RMID);
        myassert(ret != -1, "Impossible de supprimer le sémaphore tableau\n");

        ret = unlink(ECRITURE_MASTER_CLIENT);
        myassert(ret != -1, "Impossible de supprimer le tube ecriture client\n");

        ret = unlink(LECTURE_MASTER_CLIENT);
        myassert(ret != -1, "Impossible de supprimer le tube lecture client\n");
    }

    return EXIT_SUCCESS;
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien
