#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "SynchronizedQueue.h"

#define MAX_PIPE_NAME 256
#define REQUEST_PIPE_NAME "/tmp/request_pipe"
#define SHARED_MEMORY_NAME "/my_shared_memory"

// Structure envoyée au lanceur de commandes via le tube de requêtes
struct PipesNames {
    char outputPipe[MAX_PIPE_NAME];
    char errorPipe[MAX_PIPE_NAME];
};

// Structure utilisée par la fonction d'exécution des threads
struct ExecutionRequest {
    char command[MAX_MESSAGES_SIZE];
    struct PipesNames pipe_names;
};

volatile sig_atomic_t termination_requested = 0;

void termination_handler(int signum) {
    (void) signum;
    termination_requested = 1;
}

// Fonction d'exécution des threads
void *commandExecutor(void *arg) {
    struct ExecutionRequest *request = (struct ExecutionRequest *)arg;
    if (strlen(request->command) > 0) {
        printf("Executing command: %s\n", request->command);
        // Ouvrir le tube de sortie en écriture
        int output_fd = open(request->pipe_names.outputPipe, O_WRONLY);
        if (output_fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }

        // Ouvrir le tube de sortie erreur en écriture
        int error_fd = open(request->pipe_names.errorPipe, O_WRONLY);
        if (error_fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();

        switch (pid) {
        case -1:
            perror("fork");
            break;

        case 0: // Processus fils
            // Redirection de la sortie standart vers le tube outputPipe
            if (dup2(output_fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            // Fermeture du descripteur de fichier
            if (close(output_fd) == -1) {
                perror("close");
                exit(EXIT_FAILURE);
            }
            // Redirection de la sortie standart vers le tube outputPipe
            if (dup2(error_fd, STDERR_FILENO) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            // Fermeture du descripteur de fichier
            if (close(error_fd) == -1) {
                perror("close");
                exit(EXIT_FAILURE);
            }
            // Exécution de la commande en utilisant les tubes nommés
            execlp("sh", "sh", "-c", request->command, NULL);
            perror("execlp");
            exit(EXIT_FAILURE);

        default: // Processus parent
            // Attendre que le processus fils se termine
            if (waitpid(pid, NULL, 0) == -1) {
                perror("waitpid");
            }
        }
    }
    pthread_exit(NULL);
}

int main(void) {
    // Gestions des signaux
    struct sigaction act;
    if (sigemptyset(&act.sa_mask) == -1) {
        perror("sigemptyset");
    }
    act.sa_handler = termination_handler;
    act.sa_flags = 0;
    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("sigaction");
    }
    if (sigaction(SIGTERM, &act, NULL) == -1) {
        perror("sigaction");
    }

    // Création et ouverture du segment de mémoire partagée
    int shm_fd = shm_open(SHARED_MEMORY_NAME, O_CREAT | O_RDWR, 
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
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
    SynchronizedQueue *sharedQueue = (SynchronizedQueue *)mmap(NULL, sizeof(SynchronizedQueue), 
                                                            PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (sharedQueue == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    initSynchronizedQueue(sharedQueue);

    // Ouverture du tube contenant la requête 
    if (mkfifo(REQUEST_PIPE_NAME, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) == -1) {
        perror("mkfifo");            
        exit(EXIT_FAILURE);
    }

    int fd;
    if ((fd = open(REQUEST_PIPE_NAME, O_RDONLY)) == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    struct PipesNames request;
    ssize_t n;
    char command[MAX_MESSAGES_SIZE];
    
    while ((n = read(fd, &request, sizeof(request))) > 0) {
        
        // Defiler la commande
        dequeue(sharedQueue, command);

        // Allocation de la structure envoyée au thread
        struct ExecutionRequest *rq = malloc(sizeof(struct ExecutionRequest));
        if (rq == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        // Copie de la commande dans la structure
        strcpy(rq->command, command);

        // Copie des noms des tubes dans la structure
        rq->pipe_names = request;

        // Créer un thread pour chaque commande
        pthread_t thread;
        if (pthread_create(&thread, NULL, commandExecutor, (void *)rq) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }

        // Attendre la fin du thread
        if (pthread_join(thread, NULL) != 0) {
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }

        // Libéré la structure allouée
        free(rq);

        // Check for termination request
        if (termination_requested) {
            break;
        }
    }

    if (n == -1) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    if (close(fd) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    if (unlink(REQUEST_PIPE_NAME) == -1) {
        perror("unlink");
        exit(EXIT_FAILURE);
    }

    if (close(shm_fd) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    destroySynchronizedQueue(sharedQueue);

    return EXIT_SUCCESS;
}
