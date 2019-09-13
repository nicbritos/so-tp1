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
    if (ftruncate(sharedMemoryfd, sizeof(SatStruct) * filesSize) == -1) {
        perror("Could not expand shared memory object: ");
        closeMasterSharedMemory(sharedMemoryfd, sharedMemoryName);
        exit(ERROR_FTRUNCATE_FAIL);
    }

    //Map the shared memory
    size_t satStructsSize = sizeof(SatStruct) * filesSize;
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

    // Create FIFO (named pipe)
    if (mkfifo(SHARED_PIPE_SAT_NPIPE_FILE, READ_AND_WRITE_PERM) == -1) {
        perror("Could not create named pipe: ");
        closeMasterSemaphore(solvedSemaphore, sharedSemaphoreName);
        unmapSharedMemory(satStructs, satStructsSize);
        closeMasterSharedMemory(sharedMemoryfd, sharedMemoryName);
        exit(ERROR_FIFO_CREATION_FAIL);
    }

    // Open the FIFO 
    int pipefd = open(SHARED_PIPE_SAT_NPIPE_FILE, O_RDWR);
    if (pipefd == -1) {
        perror("Could not open named pipe: ");
        closeMasterSemaphore(solvedSemaphore, sharedSemaphoreName);
        unmapSharedMemory(satStructs, satStructsSize);
        closeMasterSharedMemory(sharedMemoryfd, sharedMemoryName);
        exit(ERROR_FIFO_CREATION_FAIL);
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
    char **files = argv + 1;
    int satFinished = 0;
    for (int i = 0; i < slavesQuantity; i++) {
        int forkResult = fork();
        if (forkResult > 0) {
            // Padre -- No hace nada
        } else if (forkResult == -1) {
            // error
        } else {
            // Hijo --> Instanciar cada slave (ver enunciado)
            char *arguments[2] = {SHARED_PIPE_SAT_NPIPE_FILE, NULL};
            execvp("./slave.out", arguments);
        }
    }

    // TODO: Idle processes when there are no more files to be processed
    // Aca tenemos que esperar a que haya datos. Para esto, solo podemos hacer un wait y esperar a un post.
    // Ni idea como hacer esto despues
    char inBuffer[PIPE_IN_BUFFER_SIZE] = {0};
    fd_set readfds;
    FD_SET(fd, readfds);
    while (satFinished < filesSize) {
        int ready = select(slavesQuantity, &readfds, NULL, NULL, NULL);
        if (ready == -1) {
            // ERROR
        } else if (ready != 0) {
            // PROCESS
        }
    }

    closeMasterSemaphore(solvedSemaphore, sharedSemaphoreName);
    unmapSharedMemory(satStructs, satStructsSize);
    closeMasterSharedMemory(sharedMemoryfd, sharedMemoryName);
    closePipe(pipefd);

    saveFile(outfd, filesSize, satStructs);
    close(outfd);

    exit(0);
}

int getSlavesQuantity(int filesSize) {
    return ceil(filesSize / FILES_PER_SLAVE);
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
