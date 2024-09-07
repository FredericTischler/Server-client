#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include "SynchronizedQueue.h"

#define MAX_PIPE_NAME 256
#define REQUEST_PIPE_NAME "/tmp/request_pipe"
#define OUTPUT_PIPE_NAME "/tmp/output_pipe"
#define SHARED_MEMORY_NAME "/my_shared_memory"

// Structure envoyée au lanceur de commandes via le tube de requêtes
struct PipesNames {
    char outputPipe[MAX_PIPE_NAME];
    char errorPipe[MAX_PIPE_NAME];
};

// Variable globale permettant la sortie du programme via un signal
volatile sig_atomic_t term = 1;

// Fonction exécutée par le gestionnaire de signaux
void term_handler(int signum) {
    (void) signum;
    term = 0;
}

// Fonction exécutée par les threads
void *readFromPipe(void *arg);


int main(void) {
    // Gestions des signaux
    struct sigaction act;
    if (sigemptyset(&act.sa_mask) == -1) {
        perror("sigemptyset");
    }
    act.sa_handler = term_handler;
    act.sa_flags = 0;

    // On associe l'action à SIGINT
    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Ouverture du segment de mémoire partagé
    int shm_fd = shm_open(SHARED_MEMORY_NAME, O_RDWR, 0);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    // Tronquer la taille du segment de mémoire partagée à la taille de la file synchronisée
    if (ftruncate(shm_fd, sizeof(SynchronizedQueue)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    // Mappage du segment de mémoire partagée en mémoire
    SynchronizedQueue *sharedQueue = (SynchronizedQueue *)mmap(NULL, sizeof(SynchronizedQueue), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (sharedQueue == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Ouvrir le tube de requêtes en écriture
    int request_fd = open(REQUEST_PIPE_NAME, O_WRONLY);
    if (request_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    struct PipesNames request;
    // Création du tube de sortie
    sprintf(request.outputPipe, "/tmp/output_pipe_%d", getpid());
    if (mkfifo(request.outputPipe, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    // Création du tube erreur
    sprintf(request.errorPipe, "/tmp/error_pipe_%d", getpid());
    if (mkfifo(request.errorPipe, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }


    pthread_t output_thread, error_thread;

    // Création des threads pour la lecture des tubes et écritures des résultats dans des fichiers

    if (pthread_create(&output_thread, NULL, readFromPipe, request.outputPipe) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&error_thread, NULL, readFromPipe, request.errorPipe) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    char input[MAX_MESSAGES_SIZE]; 

    while (term) {
        // Lire l'entrée de l'utilisateur
        printf("Enter command:\n");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (errno == EINTR) {
                // Si l'interruption provient d'un signal, sortir de la boucle
                break;
            } else {
                perror("fgets");
                exit(EXIT_FAILURE);
            }
        }

        // Entrée de la commande dans la file synchronisée
        enqueue(sharedQueue, input);

        // Écrire la requête dans le tube de requêtes
        if (write(request_fd, &request, sizeof(request)) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
    }

    // Interruption du thread output_thread
    if (pthread_cancel(output_thread) != 0) {
        perror("pthread_cancel");
        exit(EXIT_FAILURE);
    }

    if (pthread_join(output_thread, NULL) != 0) {
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    // Interruption du thread error_thread
    if (pthread_cancel(error_thread) != 0) {
        perror("pthread_cancel");
        exit(EXIT_FAILURE);
    }

    if (pthread_join(error_thread, NULL) != 0) {
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }


    // Fermeture des descripteurs
    if (close(request_fd) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    // Unlink les tubes créés
    if (unlink(request.outputPipe) == -1) {
        perror("unlink");
        exit(EXIT_FAILURE);
    }

    if (unlink(request.errorPipe) == -1) {
        perror("unlink");
        exit(EXIT_FAILURE);
    }

    // Fermeture et unlink du segment de mémoire partagée
    if (close(shm_fd) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    if (shm_unlink(SHARED_MEMORY_NAME) == -1) {
        perror("shm_unlink");
        exit(EXIT_FAILURE);
    }

    // Libérer la mémoire partagée
    if (munmap(sharedQueue, sizeof(SynchronizedQueue)) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

void *readFromPipe(void *arg) {
    char *pipe_name = (char *)arg;

    // Ouverture du tube
    int pipe_fd = open(pipe_name, O_RDONLY);
    if (pipe_fd == -1) {
        perror("open");
        pthread_exit(NULL);
    }

    // Ouverture du fichier
    FILE *file;
    if (strncmp(pipe_name, OUTPUT_PIPE_NAME, strlen(OUTPUT_PIPE_NAME)) == 0) {
        file = fopen("output.txt", "a");
        if (file == NULL) {
            perror("fopen");
            pthread_exit(NULL);
        }
    } else {
        file = fopen("error.txt", "a");
        if (file == NULL) {
            perror("fopen");
            pthread_exit(NULL);
        }
    }

    char buffer[MAX_MESSAGES_SIZE];
    ssize_t n;
    memset(buffer, 0, sizeof(buffer));
    // Boucle de lecture dans le tube
    while (term &&  (n = read(pipe_fd, buffer, sizeof(buffer))) > 0) {
        // Écrire dans le fichier
        if (fprintf(file, "Command result : \n%.*s\n", (int)n, buffer) < 0) {
            perror("fprintf");
            pthread_exit(NULL);
        }

        // Forcer la vidange du tampon et l'écriture immédiate dans le fichier
        if (fflush(file) != 0) {
            perror("fflush");
            pthread_exit(NULL);
        }
    }

    // Erreur de lecture
    if (n == -1) {
        perror("read");
        pthread_exit(NULL);
    }

    // Fermer le descripteur de fichier
    if (close(pipe_fd) == -1) {
        perror("close");
        pthread_exit(NULL);
    }

    // Fermer le fichier
    if (fclose(file) == EOF) {
        perror("fclose");
        pthread_exit(NULL);
    }

    pthread_exit(NULL);
}
