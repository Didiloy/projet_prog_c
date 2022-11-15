#ifndef MASTER_WORKER_H
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

#endif
