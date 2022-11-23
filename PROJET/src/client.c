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
#include <fcntl.h>
#include <math.h>
#include <pthread.h>

#include "myassert.h"

#include "master_client.h"

// chaines possibles pour le premier paramètre de la ligne de commande
#define TK_STOP "stop"
#define TK_COMPUTE "compute"
#define TK_HOW_MANY "howmany"
#define TK_HIGHEST "highest"
#define TK_LOCAL "local"

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s <ordre> [<nombre>]\n", exeName);
    fprintf(stderr, "   ordre \"" TK_STOP "\" : arrêt master\n");
    fprintf(stderr, "   ordre \"" TK_COMPUTE "\" : calcul de nombre premier\n");
    fprintf(stderr, "                       <nombre> doit être fourni\n");
    fprintf(stderr, "   ordre \"" TK_HOW_MANY "\" : combien de nombres premiers calculés\n");
    fprintf(stderr, "   ordre \"" TK_HIGHEST "\" : quel est le plus grand nombre premier calculé\n");
    fprintf(stderr, "   ordre \"" TK_LOCAL "\" : calcul de nombres premiers en local\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static int parseArgs(int argc, char *argv[], int *number)
{
    int order = ORDER_NONE;

    if ((argc != 2) && (argc != 3))
        usage(argv[0], "Nombre d'arguments incorrect");

    if (strcmp(argv[1], TK_STOP) == 0)
        order = ORDER_STOP;
    else if (strcmp(argv[1], TK_COMPUTE) == 0)
        order = ORDER_COMPUTE_PRIME;
    else if (strcmp(argv[1], TK_HOW_MANY) == 0)
        order = ORDER_HOW_MANY_PRIME;
    else if (strcmp(argv[1], TK_HIGHEST) == 0)
        order = ORDER_HIGHEST_PRIME;
    else if (strcmp(argv[1], TK_LOCAL) == 0)
        order = ORDER_COMPUTE_PRIME_LOCAL;

    if (order == ORDER_NONE)
        usage(argv[0], "ordre incorrect");
    if ((order == ORDER_STOP) && (argc != 2))
        usage(argv[0], TK_STOP " : il ne faut pas de second argument");
    if ((order == ORDER_COMPUTE_PRIME) && (argc != 3))
        usage(argv[0], TK_COMPUTE " : il faut le second argument");
    if ((order == ORDER_HOW_MANY_PRIME) && (argc != 2))
        usage(argv[0], TK_HOW_MANY " : il ne faut pas de second argument");
    if ((order == ORDER_HIGHEST_PRIME) && (argc != 2))
        usage(argv[0], TK_HIGHEST " : il ne faut pas de second argument");
    if ((order == ORDER_COMPUTE_PRIME_LOCAL) && (argc != 3))
        usage(argv[0], TK_LOCAL " : il faut le second argument");
    if ((order == ORDER_COMPUTE_PRIME) || (order == ORDER_COMPUTE_PRIME_LOCAL))
    {
        *number = strtol(argv[2], NULL, 10);
        if (*number < 2)
            usage(argv[0], "le nombre doit être >= 2");
    }

    return order;
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

int envoyerValeur(int fd_Ecriture, char *val)
{

    int valeur = atoi(val);
    write(fd_Ecriture, &valeur, sizeof(int));

    return valeur;

    /*bool tab[valeur];
    for(int i =0; i<valeur-2; i++){
        tab[i] = true;
    }
    for(int i =0; i<valeur-2; i++){
        int v = i+2;
        write(fd_Ecriture, &v, sizeof(int));

        bool retour;
        read(fd_Lecture,&retour,sizeof(bool));

        tab[i] = retour;

    }*/
}

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

typedef struct tableauClient
{
    int tailleTab;
    bool *tab;
    int semTab;
    int val;

} tableauClient;

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

/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char *argv[])
{
    int number = 0;
    int order = parseArgs(argc, argv, &number);
    // printf("order  = %d\n", order); // pour éviter le warning

    // order peut valoir 5 valeurs (cf. master_client.h) :
    //      - ORDER_COMPUTE_PRIME_LOCAL
    //      - ORDER_STOP
    //      - ORDER_COMPUTE_PRIME
    //      - ORDER_HOW_MANY_PRIME
    //      - ORDER_HIGHEST_PRIME
    //
    // si c'est ORDER_COMPUTE_PRIME_LOCAL
    //    alors c'est un code complètement à part multi-thread
    // sinon
    //    - entrer en section critique :
    //           . pour empêcher que 2 clients communiquent simultanément
    //           . le mutex est déjà créé par le master
    //    - ouvrir les tubes nommés (ils sont déjà créés par le master)
    //           . les ouvertures sont bloquantes, il faut s'assurer que
    //             le master ouvre les tubes dans le même ordre
    //    - envoyer l'ordre et les données éventuelles au master
    //    - attendre la réponse sur le second tube
    //    - sortir de la section critique
    //    - libérer les ressources (fermeture des tubes, ...)
    //    - débloquer le master grâce à un second sémaphore (cf. ci-dessous)
    //
    // Une fois que le master a envoyé la réponse au client, il se bloque
    // sur un sémaphore ; le dernier point permet donc au master de continuer
    //
    // N'hésitez pas à faire des fonctions annexes ; si la fonction main
    // ne dépassait pas une trentaine de lignes, ce serait bien.

    if (order == ORDER_COMPUTE_PRIME_LOCAL)
    {
        algoEratosthene(atoi(argv[2]));
    }
    else
    {
        // entrer en section critique
        int sem = attendrePassage();
        // ouvrir les tubes nommés
        int fd_Ecriture = open(LECTURE_MASTER_CLIENT, O_WRONLY);
        myassert(fd_Ecriture != -1, "Impossible d'ouvrire le tube écriture depuis le client");

        int fd_Lecture = open(ECRITURE_MASTER_CLIENT, O_RDONLY);
        myassert(fd_Lecture != -1, "Impossible d'ouvrire le tube lecture depuis le client");
        // envoyer l'ordre et les données éventuelles au master
        int ret = write(fd_Ecriture, &order, sizeof(int));
        myassert(ret != -1, "Impossible d'écrire dans le tube depuis le client");
        printf("J'ai envoyé l'ordre au master\n");

        if (order == ORDER_COMPUTE_PRIME)
        {
            // envoyer le nombre N
            int nombre = envoyerValeur(fd_Ecriture, argv[2]);

            // attendre la réponse sur le second tube
            int reponse;
            ret = read(fd_Lecture, &reponse, sizeof(int));
            myassert(ret != -1, "Impossible de récupérer la réponse dans le tube depuis le client");

            // débloquer les resource
            endCritique(sem, fd_Ecriture, fd_Lecture);

            // afficher résultat
            if (reponse == M_NUMBER_IS_PRIME)
            {
                printf("%d est un nombre premier", nombre);
            }
            else
            {
                printf("%d n'est pas un nombre premier", nombre);
            }
        }
        else
        {
            // attendre la réponse sur le second tube
            int reponse;
            ret = read(fd_Lecture, &reponse, sizeof(int));
            myassert(ret != -1, "Impossible de récupérer la réponse dans le tube depuis le client");

            // débloquer les resource
            endCritique(sem, fd_Ecriture, fd_Lecture);

            // afficher résultat
            afficherReponse(order, reponse);
        }
    }

    return EXIT_SUCCESS;
}
