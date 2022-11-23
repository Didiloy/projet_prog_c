#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include "myassert.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "master_worker.h"

// fonctions éventuelles proposées dans le .h
void orderStop(int writeToWorker, int receiveFromWorker, int tubeEcritureClient)
{
    printf("J'ai bien recu l'odre de m'arreter\n");
    int orderToSendToClient = W_ORDER_STOP;
    int res = write(writeToWorker, &orderToSendToClient, sizeof(int));
    myassert(res != -1, "Impossible d'envoyer un ordre au worker depuis le master\n");
    // fprintf(stderr, "j'ai transmis l'ordre au premier worker\n");

    // attendre la fin du premier worker qui lui meme attends la fin des autres worker
    wait(NULL);
    close(receiveFromWorker);
    close(writeToWorker);
    printf("Les workers se sont bien stoppés \n");
    orderToSendToClient = STOPPED; // the master will stop
    res = write(tubeEcritureClient, &orderToSendToClient, sizeof(int));
    myassert(res != -1, "Impossible d'écrire au client depuis le master\n");
}

/**
 * Calculate if the number is prime or not
 * Send the answer to the client
 * return if the the number is prime or not
 */
void orderComputePrime(int writeToWorker, int receiveFromWorker, int tubeEcritureClient, int tubeLectureClient, int *m, int *plusGrandNombrePremierCalcule)
{
    //       . récupérer le nombre N à tester provenant du client
    //       . construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
    //             il faut connaître le plus grand nombre (M) déjà envoyé aux workers
    //             on leur envoie tous les nombres entre M+1 et N-1
    //             note : chaque envoie déclenche une réponse des workers
    //       . envoyer N dans le pipeline
    //       . récupérer la réponse
    //       . la transmettre au client
    int number, responseFromWorker;
    printf("J'ai bien recu l'odre de vérifier si le nombre est premier\n");
    int ret = read(tubeLectureClient, &number, sizeof(int));
    myassert(ret != -1, "Impossible de lire dans le tube de lecture du client\n");
    printf("Je demande aux worker de vérifier si %d est premier.\n", number);
    /**
     * Si le nombre a calculer est plus petit que le plus grand nombre envoyé aux worker
     *  on envoi juste le nombre, sinon on envoi tout les nombres entre le plus grand nombre -1
     * puis après la boucle on renvoi le n pour tester si c'est premier
     * envoyé et le nombre reçu du client
     */
    if (number > *m)
    {
        // printf("je rentre pour créer de worker\n");
        for (int i = *m + 1; i <= number - 1; i++) // créer la pipeline de worker
        {
            // envoyer le nombre au premier worker
            ret = write(writeToWorker, &i, sizeof(int));
            myassert(ret != -1, "Impossible d'envoyer le nombre au worker");
            // attendre la réponse d'un des worker
            ret = read(receiveFromWorker, &responseFromWorker, sizeof(int));
            myassert(ret != -1, "Impossible de récupérer la réponse du worker");
        }
        *m = number;
    }
    // fprintf(stderr, "Je suis après le if \n");
    // envoyé le nombre au premier worker
    ret = write(writeToWorker, &number, sizeof(int));
    myassert(ret != -1, "Impossible d'envoyer le nombre au worker");
    // fprintf(stderr, "j'envoi le nombre au premier worker\n");
    // attendre la réponse d'un des worker
    ret = read(receiveFromWorker, &responseFromWorker, sizeof(int));
    myassert(ret != -1, "Impossible de récupérer la réponse du worker");
    // printf("j'ai reçu un nombre du worker %d\n", responseFromWorker);

    // Déclencher une erreur si la réponse du worker n'est pas une des réponses attendues
    if (responseFromWorker != W_IS_PRIME && responseFromWorker != W_IS_NOT_PRIME)
        myassert(responseFromWorker == W_IS_NOT_PRIME, "La réponse du worker n'est ni vrai ni faux");

    int orderToSendToClient = responseFromWorker == W_IS_PRIME ? M_NUMBER_IS_PRIME : M_NUMBER_IS_NOT_PRIME;

    // Ecrire au client si le nombre est premier ou pas
    int res = write(tubeEcritureClient, &orderToSendToClient, sizeof(int));
    myassert(res != -1, "Impossible d'écrire au client depuis le master\n");

    // changer la valeur du plus grand nombre premier calculé si le nombre est premier
    if (orderToSendToClient == M_NUMBER_IS_PRIME)
    {
        if (number > *plusGrandNombrePremierCalcule)
            *plusGrandNombrePremierCalcule = number;
    }
}

/**
 * Send a number to the client via the appropriate tube
 */
void sendNumberToClient(int tubeEcritureClient, int number)
{
    int res = write(tubeEcritureClient, &number, sizeof(int));
    myassert(res != -1, "Impossible d'écrire au client depuis le master\n");
}

/**
 * Create the semaphore to share with the client
 */
int createSemClient()
{
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
    return semClient;
}

int createSemTubeClient()
{
    key_t cleTubeClient = ftok(NOM_FICHIER_TUBE, NUMERO_TUBE);
    myassert(cleTubeClient != -1, "Impossible de créer la clé\n");

    int semTubeClient = semget(cleTubeClient, 1, IPC_CREAT | IPC_EXCL | 0641);
    // vu avec Daniel Menevaux : sémaphore
    myassert(semTubeClient != -1, "Impossible de créer le sémaphore client\n");
    return semTubeClient;
}

void createFifos()
{
    int ret = mkfifo(ECRITURE_MASTER_CLIENT, MODE);
    myassert(ret != -1, "Impossible de créer le tube ecriture client\n");

    ret = mkfifo(LECTURE_MASTER_CLIENT, MODE);
    myassert(ret != -1, "Impossible de créer le tube lecture client\n");
}

void destroy(int semClient, int semTubeClient)
{
    int ret = semctl(semClient, 0, IPC_RMID);
    myassert(ret != -1, "Impossible de supprimer le sémaphore client\n");

    ret = semctl(semTubeClient, 0, IPC_RMID);
    myassert(ret != -1, "Impossible de supprimer le sémaphore tableau\n");

    ret = unlink(ECRITURE_MASTER_CLIENT);
    myassert(ret != -1, "Impossible de supprimer le tube ecriture client\n");

    ret = unlink(LECTURE_MASTER_CLIENT);
    myassert(ret != -1, "Impossible de supprimer le tube lecture client\n");
}

void openTubes(int *lectureClient, int *ecritureClient)
{
    *lectureClient = open(LECTURE_MASTER_CLIENT, O_RDONLY);
    myassert(*lectureClient != -1, "Impossible d'ouvrir le tube de lecture du client\n");

    *ecritureClient = open(ECRITURE_MASTER_CLIENT, O_WRONLY);
    myassert(*ecritureClient != -1, "Impossible d'ouvrir le tube d'écriture pour le client\n");
}