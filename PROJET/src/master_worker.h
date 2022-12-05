/**
 * @authors Lacaze Yon - Loya Dylan
 */
#ifndef MASTER_WORKER_H
#include "master_client.h"
#include <unistd.h>
#define MASTER_WORKER_H

// On peut mettre ici des éléments propres au couple master/worker :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (écriture dans un tube, ...)

// ordres possibles pour le worker
#define W_ORDER_NONE -1
#define W_ORDER_STOP -2

// reponses possibles du worker
#define W_STOPPED -3
#define W_IS_PRIME 1
#define W_IS_NOT_PRIME 0

typedef struct DonneeWorker
{
    int valeurAssocie;
    bool aSuite;
    int fdToMaster;
    int fdToWorker;
    int workerToWorker;

} DonneeWorker;

void orderStop(int writeToWorker, int receiveFromWorker, int tubeEcritureClient);
void orderComputePrime(int writeToWorker, int receiveFromWorker, int tubeEcritureClient, int tubeLectureClient, int *m, int *plusGrandNombrePremierCalcule, int *nombreDeNombreCalcule);
void sendNumberToClient(int tubeEcritureClient, int number);
int createSemClient();
int createSemTubeClient();
void createFifos();
void destroy(int, int);
void openTubes(int *lectureClient, int *ecritureClient);
#endif
