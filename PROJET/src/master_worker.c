#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include "myassert.h"

#include "master_worker.h"

// fonctions éventuelles internes au fichier

// fonctions éventuelles proposées dans le .h
void orderStop(int writeToWorker, int receiveFromWorker, int tubeEcritureClient)
{
    printf("J'ai bien recu l'odre de m'arreter\n");
    int orderToSendToClient = W_ORDER_STOP;
    int res = write(writeToWorker, &orderToSendToClient, sizeof(int));
    // perror("");
    myassert(res != -1, "Impossible d'envoyer un ordre au worker depuis le master\n");
    fprintf(stderr, "j'ai transmis l'ordre au premier worker\n");

    int responseFromWorker;
    res = read(receiveFromWorker, &responseFromWorker, sizeof(int));
    myassert(res != -1, "Impossible de recevoir un message du worker dans le master\n'");
    // printf("reponse worker : %d", responseFromWorker);
    if (responseFromWorker == W_STOPPED)
    {
        printf("Les workers se sont bien stoppés \n");
        close(receiveFromWorker);
        orderToSendToClient = STOPPED; // the master will stop
        res = write(tubeEcritureClient, &orderToSendToClient, sizeof(int));
        myassert(res != -1, "Impossible d'écrire au client depuis le master\n");
    }
    else
    {
        // TODO si le worker ne peut pas s'arreter
        // Que faire
    }
}

/**
 * Calculate if the number is prime or not
 * Send the answer to the client
 * return if the the number is prime or not
 */
int orderComputePrime(int writeToWorker, int receiveFromWorker, int tubeEcritureClient, int tubeLectureClient, int *m, int *plusGrandNombrePremierCalcule)
{
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

    return orderToSendToClient;
}

/**
 * Send a number to the client via the appropriate tube
 */
void sendNumberToCLient(int tubeEcritureClient, int number)
{
    int res = write(tubeEcritureClient, &number, sizeof(int));
    myassert(res != -1, "Impossible d'écrire au client depuis le master\n");
}
