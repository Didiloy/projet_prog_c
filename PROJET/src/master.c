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
void loop(/* paramètres */)
{
    // boucle infinie :
    // - ouverture des tubes (cf. rq client.c)
    // - attente d'un ordre du client (via le tube nommé)
    // - si ORDER_STOP
    //       . envoyer ordre de fin au premier worker et attendre sa fin
    //       . envoyer un accusé de réception au client
    // - si ORDER_COMPUTE_PRIME
    //       . récupérer le nombre N à tester provenant du client
    //       . construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
    //             il faut connaître le plus nombre (M) déjà enovoyé aux workers
    //             on leur envoie tous les nombres entre M+1 et N-1
    //             note : chaque envoie déclenche une réponse des workers
    //       . envoyer N dans le pipeline
    //       . récupérer la réponse
    //       . la transmettre au client
    // - si ORDER_HOW_MANY_PRIME
    //       . transmettre la réponse au client
    // - si ORDER_HIGHEST_PRIME
    //       . transmettre la réponse au client
    // - fermer les tubes nommés
    // - attendre ordre du client avant de continuer (sémaphore : précédence)
    // - revenir en début de boucle
    //
    // il est important d'ouvrir et fermer les tubes nommés à chaque itération
    // voyez-vous pourquoi ?
}


/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    if (argc != 1)
        usage(argv[0], NULL);

    // - création des sémaphores
    key_t cleClient = ftok(NOM_FICHIER,NUMERO);
    myassert(cleClient != -1,"Impossible de créer la clé\n");
    
    int semClient = semget(cleClient,1,IPC_CREAT | IPC_EXCL | 0641);
    myassert(semClient != -1,"Impossible de créer le sémaphore client\n");
    
    
    
    int semTableau = semget(IPC_PRIVATE,1,IPC_CREAT | IPC_EXCL | 0641);
    myassert(semTableau != -1,"Impossible de créer le sémaphore tableau\n");
    
    // - création des tubes nommés
    int ret = mkfifo(ECRITURE_CLIENT,MODE);
    myassert(ret != -1,"Impossible de créer le tube ecriture client\n");
    
    ret = mkfifo(LECTURE_CLIENT,MODE);
    myassert(ret != -1,"Impossible de créer le tube lecture client\n");
    
    // - création du premier worker
    
    int fdToMaster[2];
    ret = pipe(fdToMaster);
    myassert(ret != -1,"Impossible de créer le tube lecture worker\n");
    close(fdToMaster[1]);
    
    int fds[2];
    ret = pipe(fds);
    myassert(ret != -1,"Impossible de créer le tube ecriture worker\n");
    close(fds[0]);
    
    
	ret = fork();
	if(ret == 0){
		char * ecriture;							//TODO convertir le entrrée des pipe non férmer en string pour le passes en parametre
		
		execv("worker",["worker","2",,,"\0"])
	}
    

    // boucle infinie
    loop(/* paramètres */);

    // destruction des tubes nommés, des sémaphores, ...
    ret = semctl(semClient,0,IPC_RMID);
    myassert(ret != -1,"Impossible de supprimer le sémaphore client\n");
    
    ret = semctl(semTableau,0,IPC_RMID);
    myassert(ret != -1,"Impossible de supprimer le sémaphore tableau\n");
    
    ret = unlink(ECRITURE_CLIENT);
    myassert(ret != -1,"Impossible de supprimer le tube ecriture client\n");
    
    ret = unlink(LECTURE_CLIENT);
    myassert(ret != -1,"Impossible de supprimer le tube lecture client\n");

    return EXIT_SUCCESS;
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien
