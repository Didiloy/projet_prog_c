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
    printf("Dans la boucle \n");
    int m = 2; // Plus grand nombre envoyé aux worker. 2 de base car on crée le premier worker avec 2
    int nombreDeNombreCalcule = 0;
    int plusGrandNombrePremierCalcule = 0;
    // boucle infinie :
    bool infini = true;
    while (infini)
    {
        // - ouverture des tubes (cf. rq client.c)
        int tubeLectureClient = open(LECTURE_MASTER_CLIENT, O_RDONLY);
        myassert(tubeLectureClient != -1, "Impossible d'ouvrir le tube de lecture du client\n");

        int tubeEcritureClient = open(ECRITURE_MASTER_CLIENT, O_WRONLY);
        myassert(tubeEcritureClient != -1, "Impossible d'ouvrir le tube d'écriture pour le client\n");

        // - attente d'un ordre du client (via le tube nommé)
        int order;
        ssize_t ret = read(tubeLectureClient, &order, sizeof(int));
        myassert(ret != -1, "Impossible de lire dans le tube de lecture du client\n");

        int responseFromWorker, res, orderToSendToClient, number;
        printf("j'ai recu un ordre du client %d\n", order);

        switch (order)
        {
        // - si ORDER_STOP
        //       . envoyer ordre de fin au premier worker et attendre sa fin
        //       . envoyer un accusé de réception au client
        case ORDER_STOP:
            printf("J'ai bien recu l'odre de m'arreter\n");
            orderToSendToClient = W_ORDER_STOP;
            res = write(writeToWorker, &orderToSendToClient, sizeof(int));
            myassert(res != -1, "Impossible d'envoyer un ordre au worker depuis le master\n");
            fprintf(stderr, "j'ai écrit");
            res = read(receiveFromWorker, &responseFromWorker, sizeof(int));
            myassert(res != -1, "Impossible de recevoir un message du worker dans le master\n'");
            printf("reponse worker : %d", responseFromWorker);
            if (responseFromWorker == W_STOPPED)
            {
                printf("recu l'arret du worker");
                close(receiveFromWorker);
                orderToSendToClient = STOPPED; // the master will stop
                res = write(tubeEcritureClient, &orderToSendToClient, sizeof(int));
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
        //             il faut connaître le plus grand nombre (M) déjà envoyé aux workers
        //             on leur envoie tous les nombres entre M+1 et N-1
        //             note : chaque envoie déclenche une réponse des workers
        //       . envoyer N dans le pipeline
        //       . récupérer la réponse
        //       . la transmettre au client
        case ORDER_COMPUTE_PRIME:
            printf("J'ai bien recu l'odre de vérifier si le nombre est premier\n");
            ret = read(tubeLectureClient, &number, sizeof(int));
            myassert(ret != -1, "Impossible de lire dans le tube de lecture du client\n");
            printf("Je demande aux worker de vérifier si %d est premier.\n", number);
            /**
             * Si le nombre a calculer est plus petit que le plus grand nombre envoyé aux worker
             *  on envoi juste le nombre, sinon on envoi tout les nombres entre le plus grand nombre -1
             * puis après la boucle on renvoi le n pour tester si c'est premier
             * envoyé et le nombre reçu du client
             */
            if (number > m)
            {
                printf("je rentre pour créer de worker\n");
                for (int i = m + 1; i <= number - 1; i++) // créer la pipeline de worker
                {
                    // envoyer le nombre au premier worker
                    ret = write(writeToWorker, &i, sizeof(int));
                    myassert(ret != -1, "Impossible d'envoyer le nombre au worker");
                    // attendre la réponse d'un des worker
                    ret = read(receiveFromWorker, &responseFromWorker, sizeof(int));
                    myassert(ret != -1, "Impossible de récupérer la réponse du worker");
                }
                m = number;
            }
            fprintf(stderr, "Je suis après le if \n");
            // envoyé le nombre au premier worker
            ret = write(writeToWorker, &number, sizeof(int));
            perror("");
            myassert(ret != -1, "Impossible d'envoyer le nombre au worker");
            fprintf(stderr, "j'envoi le nombre au premier worker\n");
            // attendre la réponse d'un des worker
            ret = read(receiveFromWorker, &responseFromWorker, sizeof(int));
            myassert(ret != -1, "Impossible de récupérer la réponse du worker");
            printf("j'ai reçu un nombre du worker %d\n", responseFromWorker);

            // Déclencher une erreur si la réponse du worker n'est pas une des réponses attendues
            if (responseFromWorker != W_IS_PRIME && responseFromWorker != W_IS_NOT_PRIME)
                myassert(responseFromWorker == W_IS_NOT_PRIME, "La réponse du worker n'est ni vrai ni faux");

            orderToSendToClient = responseFromWorker == W_IS_PRIME ? M_NUMBER_IS_PRIME : M_NUMBER_IS_NOT_PRIME;

            // Ecrire au client si le nombre est premier ou pas
            res = write(tubeEcritureClient, &orderToSendToClient, sizeof(int));
            myassert(res != -1, "Impossible d'écrire au client depuis le master\n");

            // ajouter +1 au nombre de nombres premier calculés par le master pour pouvoir l'envoyer en cas de demande du client
            nombreDeNombreCalcule += 1;

            // changer la valeur du plus grand nombre premier calculé si le nombre est premier
            if (orderToSendToClient == W_IS_PRIME)
            {
                if (number > plusGrandNombrePremierCalcule)
                    plusGrandNombrePremierCalcule = number;
            }
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

    int semTableau = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | 0641);
    myassert(semTableau != -1, "Impossible de créer le sémaphore tableau\n");
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
    myassert(ret != -1, "Impossible de créer le tube lecture vers le master du worker\n");
    printf("fdTomaster[0] : %d , fdToMaster[1]: %d\n", fdToMaster[0], fdToMaster[1]);

    int fds[2];
    ret = pipe(fds);
    myassert(ret != -1, "Impossible de créer le tube ecriture du master vers le worker\n");

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
        close(fdToMaster[1]);
        close(fds[0]);

        int tmp;
        ret = read(fdToMaster[0], &tmp, sizeof(int));
        myassert(ret != -1, "Impossible de récupérer la réponse du worker");
        // boucle infinie
        loop(fds[1], fdToMaster[0]);
        // destruction des tubes nommés, des sémaphores, ...
        ret = semctl(semClient, 0, IPC_RMID);
        myassert(ret != -1, "Impossible de supprimer le sémaphore client\n");

        ret = semctl(semTableau, 0, IPC_RMID);
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
