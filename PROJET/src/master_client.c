/**
 * @authors Lacaze Yon - Loya Dylan
 */
#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "myassert.h"

#include "master_client.h"

// fonctions éventuelles internes au fichier

// fonctions éventuelles proposées dans le .h
void endCritique(int sem, int ecriture, int lecture)
{

    int ret = close(ecriture);
    myassert(ret == 0, "Impossible de fermer le tube écriture dans le client");

    ret = close(lecture);
    myassert(ret == 0, "Impossible de fermer le tube lecture dans le client");

    // libérer le master une fois les tube férmer
    key_t cleClient = ftok(NOM_FICHIER_TUBE, NUMERO_TUBE);
    myassert(cleClient != -1, "Impossible de créer la clé\n");

    int semClient = semget(cleClient, 1, 0);
    myassert(semClient != -1, "Impossible de créer le sémaphore client\n");

    struct sembuf operation1 = {0, +1, 0};

    ret = semop(semClient, &operation1, 1);
    myassert(ret != -1, "Impossible de faire une opération sur la sémaphore depuis le client");

    // libérer la place pour un autre client
    struct sembuf operation2 = {0, +1, 0};

    ret = semop(sem, &operation2, 1);
    myassert(ret != -1, "Impossible de faire une opération sur la sémaphore depuis le client");
}

void afficherReponse(int order, int reponse)
{
    switch (order)
    {
    case ORDER_STOP:
        if (reponse == STOPPED)
        {
            printf("le master s'est bien stoppé");
        }
        else
        {
            printf("le master ne s'est pas stoppé");
        }
        break;
    case ORDER_HOW_MANY_PRIME:
        printf("il y a eu %d demande", reponse);
        break;
    case ORDER_HIGHEST_PRIME:
        if (reponse == 0)
            printf("Vous n'avez calculé aucun nombre premier\n");
        else
            printf("le nombre premier le plus grand demandé est %d\n", reponse);
        break;
    }
}

int attendrePassage()
{
    key_t cleClient = ftok(NOM_FICHIER, NUMERO);
    myassert(cleClient != -1, "Impossible de créer la clé\n");

    int semClient = semget(cleClient, 1, 0);
    myassert(semClient != -1, "Impossible de créer le sémaphore client\n");

    struct sembuf operation;
    operation.sem_num = 0;
    operation.sem_op = -1;
    operation.sem_flg = 0;

    int ret = semop(semClient, &operation, 1);
    myassert(ret != -1, "Impossible de faire une opération sur le sémaphore depuis le client");
    // fprintf(stderr, "-1 Bien fait sur le sémaphore %d\n", semClient);
    return semClient;
}

void *threadTableau(void *s)
{
    tableauClient donnee = *(tableauClient *)s;

    struct sembuf operation = {0, -1, 0};

    int ret = semop(donnee.semTab, &operation, 1);
    myassert(ret != -1, "Impossible de faire une opération sur la sémaphore du tableau depuis le thread");

    for (int i = 1; donnee.val * i < donnee.tailleTab - 1; i++)
    {
        donnee.tab[i * donnee.val] = false;
    }

    struct sembuf operation2 = {0, 1, 0};

    ret = semop(donnee.semTab, &operation2, 1);
    myassert(ret != -1, "Impossible de faire une opération sur la sémaphore du tableau depuis le thread");

    return NULL;
}

void algoEratosthene(int N)
{
    tableauClient s;
    s.tab = malloc(sizeof(bool) * (N - 1));

    key_t cletab = ftok(NOM_FICHIER_TABLEAU, NUMERO_TABLEAU);
    myassert(cletab != -1, "Impossible de créer la clé pour le tableau\n");

    s.semTab = semget(cletab, 1, 0);
    myassert(s.semTab != -1, "Impossible de créer le sémaphore pour le tableau\n");

    struct sembuf operation = {0, +1, 0};

    int ret = semop(s.semTab, &operation, 1);
    myassert(ret != -1, "Impossible de faire une opération sur la sémaphore du tableau depuis le client");

    for (int i = 0; i < N - 1; i++)
    {
        s.tab[i] = true;
    }
    s.tailleTab = N - 1;

    pthread_t *tableau;
    // tableau = malloc((sqrt(N) - 1) * sizeof(int));
    tableau = malloc((N) * sizeof(int)); // TODO quelle est la bonne taille ?

    for (int i = 0; i < sqrt(N) - 1; i++)
    {
        // fprintf(stderr, "thread %d créé \n", i);
        s.val = i + 2;
        pthread_create(&tableau[i], NULL, threadTableau, &s);
    }

    // fprintf(stderr, "Attendre les thread\n");
    for (int i = 0; i < sqrt(N) - 1; i++)
    {
        // fprintf(stderr, "j'attends %d qui est le thread %ld\n", i, tableau[i]);
        ret = pthread_join(tableau[i], NULL);
        // perror("");
        myassert(ret == 0, "Impossible d'attendre les thread");
        // fprintf(stderr, "j'ai fini d'attendre %d\n", i);
    }

    // fprintf(stderr, "Je passe pas ici\n");
    for (int i = 0; i < N - 1; i++)
    {
        if (s.tab[i] == true)
        {
            int j = i + 2;
            printf("%d est premier\n", j);
        }
    }

    free(tableau);
}