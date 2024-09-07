#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#ifndef SYNCHRONIZED_QUEUE_H
#define SYNCHRONIZED_QUEUE_H

#define MAX_MESSAGES 10
#define MAX_MESSAGES_SIZE 1024

typedef struct {
    char messages[MAX_MESSAGES][MAX_MESSAGES_SIZE];
    size_t front;  // Indice du premier élément
    size_t rear;   // Indice du prochain emplacement disponible pour l'enfilage
    sem_t mutex;   // Sémaphore pour la synchronisation
    sem_t empty;   // Sémaphore pour gérer les emplacements vides dans la file
    sem_t full;    // Sémaphore pour gérer les emplacements pleins dans la file
} SynchronizedQueue;

/**
 * Initialise une file synchronisée.
 * @param queue: Pointeur vers la file synchronisée à initialiser.
 */
void initSynchronizedQueue(SynchronizedQueue *queue);

/**
 * Enfile un message dans la file synchronisée.
 * @param queue: Pointeur vers la file synchronisée.
 * @param message: Message à enfiler.
 */
void enqueue(SynchronizedQueue *queue, const char *message);

/**
 * Défile un message de la file synchronisée.
 * @param queue: Pointeur vers la file synchronisée.
 * @param message: Pointeur vers la chaine de charactère à initialisé avec le message à défiler
 * 
 */
void dequeue(SynchronizedQueue *queue, char *message);

/**
 * Libère les ressources associées à la file synchronisée.
 * @param queue: Pointeur vers la file synchronisée à détruire.
 */
void destroySynchronizedQueue(SynchronizedQueue *queue);


#endif  // SYNCHRONIZED_QUEUE_H

