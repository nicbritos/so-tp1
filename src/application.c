#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/select.h>
#include <string.h>
#include "utils/utils.h"
#include "utils/satStruct.h"
#include "application.h"

#define SHARED_SAT_MEMORY_NAME "/tp1mem%lu"
#define SHARED_SAT_DATA_SEMAPHORE_NAME "/tp1sem%lu"
#define SHARED_PIPE_SAT_NPIPE_FILE "/tmp/tp1NamedPipe"
#define OUT_FILE "./tp1_output.txt"

#define PIPE_IN_BUFFER_SIZE 4096
#define READ_AND_WRITE_PERM 0666
#define FILES_PER_SLAVE 10.0
#define FILES_MIN_PERCENTAGE 0.05 
#define MAX_SHARED_MEMORY_NAME_LENGTH 256
#define MAX_SEMAPHORE_NAME_LENGTH 256
#define MAX_SLAVES 512 // Solves problem of "Too many files open"

#define ERROR_NO 0
#define ERROR_NO_FILES -1
#define ERROR_SHMOPEN_FAIL -2
#define ERROR_FTRUNCATE_FAIL -3
#define ERROR_MMAP_FAIL -4
#define ERROR_SEMOPEN_FAIL -5
#define ERROR_FIFO_CREATION_FAIL -6
#define ERROR_FIFO_OPEN_FAIL -7
#define ERROR_FILE_OPEN_FAIL -8

int main(int argc, char **argv) {
    int filesSize = argc - 1;
    if (filesSize < 1) {
        printError("You need to provide at least one CNF file.");
        exit(ERROR_NO_FILES);
    }

    pid_t pid = getpid();

    //Create shared memory
    char *sharedMemoryName = calloc(1, sizeof(char) * MAX_SHARED_MEMORY_NAME_LENGTH);
    snprintf(sharedMemoryName, MAX_SHARED_MEMORY_NAME_LENGTH, SHARED_SAT_MEMORY_NAME, pid);
    int sharedMemoryfd = shm_open(sharedMemoryName, O_CREAT | O_RDWR, READ_AND_WRITE_PERM);
    if (sharedMemoryfd == -1) {
        perror("Could not create shared memory object: ");
        exit(ERROR_SHMOPEN_FAIL);
    }

    //Reserve storage for shared memory
    size_t satStructsSize = sizeof(SatStruct) * (filesSize + 1); // One more for the terminating struct
    if (ftruncate(sharedMemoryfd, satStructsSize) == -1) {
        perror("Could not expand shared memory object: ");
        closeMasterSharedMemory(sharedMemoryfd, sharedMemoryName);
        exit(ERROR_FTRUNCATE_FAIL);
    }

    //Map the shared memory
    SatStruct *satStructs = (SatStruct*) mmap(NULL, satStructsSize, PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemoryfd, 0);
    if (satStructs == NULL) {
        perror("Could not map shared memory: ");
        closeMasterSharedMemory(sharedMemoryfd, sharedMemoryName);
        exit(ERROR_MMAP_FAIL);
    }

    //Create shared semaphore (View)
    char *sharedSemaphoreName = calloc(1, sizeof(char) * MAX_SEMAPHORE_NAME_LENGTH);
    snprintf(sharedSemaphoreName, MAX_SEMAPHORE_NAME_LENGTH, SHARED_SAT_DATA_SEMAPHORE_NAME, pid);
    sem_t *solvedSemaphore = sem_open(sharedSemaphoreName, O_CREAT | O_RDWR);
    if (solvedSemaphore == SEM_FAILED) {
        perror("Could not create shared semaphore: ");
        unmapSharedMemory(satStructs, satStructsSize);
        closeMasterSharedMemory(sharedMemoryfd, sharedMemoryName);
        exit(ERROR_SEMOPEN_FAIL);
    }

    //Create the output file
    int outfd = open(OUT_FILE, O_CREAT | O_RDONLY, READ_AND_WRITE_PERM);
    if (outfd == -1) {
        perror("Could not create output file: ");
        closeMasterSemaphore(solvedSemaphore, sharedSemaphoreName);
        unmapSharedMemory(satStructs, satStructsSize);
        closeMasterSharedMemory(sharedMemoryfd, sharedMemoryName);
        closePipe(pipefd);
        exit(ERROR_FILE_OPEN_FAIL);
    }

    // Para proceso Vista
    // sleep(2)
    printf("%lu", pid);

    int slavesQuantity = getSlavesQuantity(filesSize);
    int digitQuantity = digits(slavesQuantity);
    int baseLength = strlen(SHARED_PIPE_SAT_NPIPE_FILE);
    char **files = argv + 1;
    int filesSent = 0;
    int satFinished = 0;
    char *pipeNames[slavesQuantity];
    int *pipesfds[slavesQuantity];
    fd_set readfds;
    FD_ZERO(&readfds);
    for (int i = 0; i < slavesQuantity; i++) {
        // Create Pipe Name
        char *pipeName = malloc(sizeof(*pipeName) * (baseLength + digitQuantity + 1));
        strcpy(pipeName, SHARED_PIPE_SAT_NPIPE_FILE);
        sprintf(pipeName + baseLength + digitQuantity - digits(i), "%d", i);
        for (int j = 0; j < digitQuantity - digits(i); j++) {
            pipeName[baseLength + j] = '0';
        }

        int forkResult = fork();
        if (forkResult > 0) {
            // Padre
            int fd = createAndOpenPipe();
            if (fd == -1) {
                // Error. TERMINAR
            }
            FD_SET(fd, &readfds);
            pipeNames[i] = pipeName;
            pipesfds[i] = fd;

            sendFile(fd, files, &filesSent);
            sendFile(fd, files, &filesSent);
        } else if (forkResult == -1) {
            // error. TERMINAR
        } else {
            // Hijo --> Instanciar cada slave (ver enunciado)
            char *arguments[2] = {SHARED_PIPE_SAT_NPIPE_FILE, NULL};
            if (execvp("./src/slave", arguments) == -1) {
                // Error. TERMINAR
            }
        }
    }

    while (satFinished < filesSize) {
        int ready = select(slavesQuantity, &readfds, NULL, NULL, NULL);
        if (ready == -1) {
            // ERROR
        } else if (ready != 0) {
            for (int i = 0; i < slavesQuantity; i++) {
                int fd = pipesfds[i];

                if (FD_ISSET(fd, &readfds)) {
                    FD_CLR(fd, &readfds);

                    processInput(fd, satStructs, satFinished++);
                    if (filesSent < filesSize) {
                        sendFile(fd, files, &filesSent);
                    } else {
                        terminateSlave(fd);
                    }
                    sem_post(solvedSemaphore);
                }
            }
        }
    }

    terminateView(satStructs, satFinished, solvedSemaphore);

    closeMasterSemaphore(solvedSemaphore, sharedSemaphoreName);
    unmapSharedMemory(satStructs, satStructsSize);
    closeMasterSharedMemory(sharedMemoryfd, sharedMemoryName);
    closePipe(pipefd);

    saveFile(outfd, filesSize, satStructs);
    close(outfd);

    exit(0);
}

int getSlavesQuantity(int filesSize) {
    return min(ceil(filesSize / FILES_PER_SLAVE), MAX_SLAVES);
}

int getMinFilesQuantity(int filesSize){
    return min(floor(filesSize * FILES_MIN_PERCENTAGE), floor(FILES_PER_SLAVE/2));
}

void saveFile(int fd, int count, SatStruct *satStructs) {
    dprintf(fd, "TP1 - MINISAT output for %d files\n", count);
    for (int i = 0; i < count; i++) {
        dumpResults(fd, satStructs + i);
    }
}

int createAndOpenPipe(char *name) {
    // Create FIFO (named pipe)
    if (mkfifo(name, READ_AND_WRITE_PERM) == -1) {
        perror("Could not create named pipe: ");
        closeMasterSemaphore(solvedSemaphore, sharedSemaphoreName);
        unmapSharedMemory(satStructs, satStructsSize);
        closeMasterSharedMemory(sharedMemoryfd, sharedMemoryName);
        exit(ERROR_FIFO_CREATION_FAIL);
    }

    // Open the FIFO 
    int pipefd = open(name, O_RDWR);
    if (pipefd == -1) {
        perror("Could not open named pipe: ");
        closeMasterSemaphore(solvedSemaphore, sharedSemaphoreName);
        unmapSharedMemory(satStructs, satStructsSize);
        closeMasterSharedMemory(sharedMemoryfd, sharedMemoryName);
        exit(ERROR_FIFO_CREATION_FAIL);
    }

    return pipefd;
}

void sendFile(int fd, char *str, int *filesSent) {
    write(fd, files[*filesSent], strlen(files[*filesSent]) + 1);
    (*filesSent)++;
}

// TODO: UTILS.C
void processInput(int fd, SatStruct *satStructs, int index) {
    char buf[1] = {NULL};
    SatStruct satStruct = satStructs + index;
    while (read(fd, buf, 1) == 1) {
        // foo.cnf\n123\n45\n1\n67
    }

    read
}

void terminateSlave(int fd) {
    write(fd, NULL, 1);
}

void terminateView(SatStruct *satStructs, int count, sem_t *solvedSemaphore) {
    SatStruct satStruct = satStructs + count;
    satStruct->filename = NULL;
    sem_post(solvedSemaphore);
}
