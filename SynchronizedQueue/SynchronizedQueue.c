#include "SynchronizedQueue.h"

void initSynchronizedQueue(SynchronizedQueue *queue) {
    queue->front = queue->rear = 0;
    sem_init(&queue->mutex, 0, 1);
    sem_init(&queue->empty, 0, MAX_MESSAGES);
    sem_init(&queue->full, 0, 0);
}

void enqueue(SynchronizedQueue *queue, const char *message) {
    sem_wait(&queue->empty);
    sem_wait(&queue->mutex);

    strcpy(queue->messages[queue->rear], message);
    queue->rear = (queue->rear + 1) % MAX_MESSAGES;

    sem_post(&queue->mutex);
    sem_post(&queue->full);
}

void dequeue(SynchronizedQueue *queue, char *message) {
    sem_wait(&queue->full);
    sem_wait(&queue->mutex);

    strcpy(message, queue->messages[queue->front]);
    queue->front = (queue->front + 1) % MAX_MESSAGES;

    sem_post(&queue->mutex);
    sem_post(&queue->empty);
}

void destroySynchronizedQueue(SynchronizedQueue *queue) {
    sem_destroy(&queue->mutex);
    sem_destroy(&queue->empty);
    sem_destroy(&queue->full);
}