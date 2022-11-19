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