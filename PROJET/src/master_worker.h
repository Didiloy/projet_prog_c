#ifndef MASTER_WORKER_H
#define MASTER_WORKER_H

// On peut mettre ici des éléments propres au couple master/worker :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (écriture dans un tube, ...)

// ordres possibles pour le worker
#define W_ORDER_NONE -1
#define W_ORDER_STOP -2
#define W_STOPPED -3

typedef struct DonneeWorker{
    int valeurAssocie;
    bool aSuite;
    int fdToMaster;
    int fdToWorker;
    int workerToWorker;
    
}DonneeWorker;

#endif
