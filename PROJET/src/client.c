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
    myassert(ret != -1, "Impossible de faire une opération sur la sémaphore depuis le clien");

    return semClient;
}

int envoyerValeurAlea(int fd_Ecriture)
{
    // TODO générer une valeur aléatoire et l'envoyer au master et retourner la valeur envoyer
    return 0;
}

void endCritique(int sem, int ecriture, int lecture)
{

    int ret = close(ecriture);
    myassert(ret == 0, "Impossible de fermer le tube écriture dans le client");

    ret = close(lecture);
    myassert(ret == 0, "Impossible de fermer le tube lecture dans le client");

    struct sembuf operation;
    operation.sem_num = 0;
    operation.sem_op = +1;
    operation.sem_flg = 0;

    ret = semop(sem, &operation, 1);
    myassert(ret != -1, "Impossible de faire une opération sur la sémaphore depuis le clien");
}

void afficherReponse(int order, int reponse)
{
    switch (order)
    {
    case ORDER_STOP:
        if (reponse == STOPPED)
        {
            printf("le master c'est bien arréter");
        }
        else
        {
            printf("le master ne c'est pas arréter");
        }
        break;
    case ORDER_HOW_MANY_PRIME:
        printf("il y a eu %d demande", reponse);
        break;
    case ORDER_HIGHEST_PRIME:
        printf("le nombre le plus grand demander est %d", reponse);
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
    printf("order  = %d\n", order); // pour éviter le warning

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
        // TODO Je sais pas
    }
    else
    {
        // entrer en section critique
        int sem = attendrePassage();

        // ouvrir les tubes nommés
        fprintf(stderr, "Je crée le fd ecriture\n");
        int fd_Ecriture = open(LECTURE_MASTER_CLIENT, O_WRONLY);
        fprintf(stderr, "J'ai créé le fd ecriture\n");
        myassert(fd_Ecriture != -1, "Impossible d'ouvrire le tube écriture depuis le client");

        printf("Je crée le fd lecture\n");
        int fd_Lecture = open(ECRITURE_MASTER_CLIENT, O_RDONLY);
        myassert(fd_Lecture != -1, "Impossible d'ouvrire le tube lecture depuis le client");
        // envoyer l'ordre et les données éventuelles au master
        printf("Juste avant le write\n");
        int ret = write(fd_Ecriture, &order, sizeof(int));
        myassert(ret != -1, "Impossible d'écrire dans le tube depuis le client");
        printf("J'ai envoyé l'ordre au master\n");

        if (order == ORDER_COMPUTE_PRIME)
        {
            // envoyer le nombre N
            int nombre = envoyerValeurAlea(fd_Ecriture);

            // attendre la réponse sur le second tube
            bool reponse;
            ret = read(fd_Lecture, &reponse, sizeof(bool));
            myassert(ret != -1, "Impossible de récupérer la réponse dans le tube depuis le client");

            // débloquer les resource
            endCritique(sem, fd_Ecriture, fd_Lecture);

            // afficher résultat
            if (reponse == true)
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
